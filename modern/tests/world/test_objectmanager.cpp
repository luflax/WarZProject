// ObjectManager lifecycle.
//
// This is the contract the whole codebase is written against and nothing verifies it.
// Two properties matter:
//
//   DEFERRED DELETION. setActiveFlag(0) does not delete. EndFrame() runs the object's
//   teardown, sets the flag to -1, and then counts down one per frame until -4, at
//   which point the object is really freed (ObjManag.cpp:1161-1204). That is why
//   CLAUDE.md tells you a pointer stays valid for the rest of the frame it was deleted
//   in -- and why gameplay code all over the tree keeps raw GameObject* across calls.
//   If the countdown ever shortens, that assumption becomes a use-after-free that
//   reproduces once an hour under load and never in a debugger.
//
//   CLASS REGISTRATION. Objects are constructed from a STRING naming their class
//   (AUTOREGISTER_CLASS, AObject.h:22-49), which is how LevelData.xml can name a type.
//   That indirection means nothing references most classes by symbol, so they are
//   exactly the thing a linker drops -- see the note in server/src/CMakeLists.txt about
//   why these sources are compiled into the test rather than archived.
//
// NOTE ON EXECUTION: these tests are PE32 and need a working Wine to run. In a
// container where wine32:i386 is unavailable, CMake disables the suite and this file is
// still compiled and linked, which by itself proves the API usage and the class-table
// linkage. See tests/CMakeLists.txt.

#include "warz_test.h"

#include "r3dPCH.h"
#include "r3d.h"

#include "gameobjects/ObjManag.h"
#include "gameobjects/GameObj.h"

namespace {

// Brings a bare ObjectManager up and tears it down around one test. The world is a
// process-wide singleton (GameWorld()), so tests must not leak objects into each other;
// the destructor is what keeps them independent.
struct WorldFixture {
    WorldFixture()
    {
        GameWorld_Create();
        // Far smaller than the shipping 16383 / 49152. The pools are flat arrays that
        // EndFrame() walks in full every frame, and a test does not need to pay for
        // 65k slots to observe the lifecycle.
        GameWorld().Init(256, 256);
    }

    ~WorldFixture()
    {
        GameWorld().Destroy();
        GameWorld_Destroy();
    }
};

// One EndFrame, plus the frame-time bookkeeping EndFrame reads. r3dEndFrame/StartFrame
// are what the real loop calls around it (ServerGame.cpp:176-185).
void tick()
{
    r3dEndFrame();
    r3dStartFrame();
    GameWorld().EndFrame();
}

} // namespace

WARZ_TEST(objectmanager, starts_empty)
{
    WorldFixture w;
    CHECK_EQ(GameWorld().GetNumObjects(), 0);
    CHECK_EQ(GameWorld().GetStaticObjectCount(), 0);
}

WARZ_TEST(objectmanager, creating_by_class_name_finds_the_registered_class)
{
    // MeshGameObject is registered by AUTOREGISTER_CLASS and is the type the level
    // loader instantiates most. If this returns null, the class table is empty --
    // which is what a dropped static initialiser looks like, and it would make every
    // level load produce an empty world with no error anywhere.
    WorldFixture w;

    GameObject* obj = srv_CreateGameObject("MeshGameObject", "unit_test_mesh",
                                           r3dPoint3D(1.0f, 2.0f, 3.0f));
    REQUIRE(obj != nullptr);
    CHECK_EQ(GameWorld().GetNumObjects(), 1);

    CHECK_NEAR(obj->GetPosition().x, 1.0f);
    CHECK_NEAR(obj->GetPosition().y, 2.0f);
    CHECK_NEAR(obj->GetPosition().z, 3.0f);
}

WARZ_TEST(objectmanager, unknown_class_name_returns_null_rather_than_crashing)
{
    // LevelData.xml is data, and data can name a class that no longer exists. The
    // loader has to survive that -- a level authored against a newer build must not
    // take the server down.
    WorldFixture w;

    GameObject* obj = srv_CreateGameObject("obj_ThisClassDoesNotExist", "bogus",
                                           r3dPoint3D(0.0f, 0.0f, 0.0f));
    CHECK(obj == nullptr);
    CHECK_EQ(GameWorld().GetNumObjects(), 0);
}

WARZ_TEST(objectmanager, lookup_by_id_round_trips)
{
    WorldFixture w;

    GameObject* obj = srv_CreateGameObject("MeshGameObject", "findme",
                                           r3dPoint3D(0.0f, 0.0f, 0.0f));
    REQUIRE(obj != nullptr);

    CHECK(GameWorld().GetObject(obj->ID) == obj);
}

WARZ_TEST(objectmanager, deleted_object_survives_the_frame_it_was_deleted_in)
{
    // THE assumption the codebase is built on. setActiveFlag(0) must not free anything
    // synchronously -- callers keep using the pointer for the rest of the frame.
    WorldFixture w;

    GameObject* obj = srv_CreateGameObject("MeshGameObject", "doomed",
                                           r3dPoint3D(0.0f, 0.0f, 0.0f));
    REQUIRE(obj != nullptr);
    const gobjid_t id = obj->ID;

    obj->setActiveFlag(0);

    // Still there, still findable, before any EndFrame has run.
    CHECK_EQ(GameWorld().GetNumObjects(), 1);
    CHECK(GameWorld().GetObject(id) == obj);
}

WARZ_TEST(objectmanager, deletion_takes_exactly_four_frames)
{
    // The countdown in ObjManag.cpp:1165-1177: EndFrame sees activeFlag 0, runs the
    // teardown and sets -1; then -2, -3, and on reaching -4 calls DeleteObject. So the
    // object outlives three EndFrames and is gone after the fourth.
    //
    // Pinning the exact number matters in both directions. Shorter is a use-after-free.
    // Longer means an object that has already run OnDestroy stays visible to iteration
    // for extra frames, which is its own class of bug.
    WorldFixture w;

    GameObject* obj = srv_CreateGameObject("MeshGameObject", "countdown",
                                           r3dPoint3D(0.0f, 0.0f, 0.0f));
    REQUIRE(obj != nullptr);
    const gobjid_t id = obj->ID;

    obj->setActiveFlag(0);

    tick();   // 0 -> -1, teardown runs
    CHECK(GameWorld().GetObject(id) != nullptr);
    tick();   // -1 -> -2
    CHECK(GameWorld().GetObject(id) != nullptr);
    tick();   // -2 -> -3
    CHECK(GameWorld().GetObject(id) != nullptr);
    tick();   // -3 -> -4, freed
    CHECK(GameWorld().GetObject(id) == nullptr);
    CHECK_EQ(GameWorld().GetNumObjects(), 0);
}

WARZ_TEST(objectmanager, many_objects_delete_without_disturbing_their_neighbours)
{
    // EndFrame walks the pool by index and mutates it while iterating. Deleting a
    // subset must leave the rest untouched -- an off-by-one in that walk shows up as
    // objects vanishing at random, which is very hard to attribute after the fact.
    WorldFixture w;

    const int COUNT = 16;
    GameObject* objs[COUNT] = {};
    gobjid_t    ids[COUNT]  = {};

    for (int i = 0; i < COUNT; ++i) {
        objs[i] = srv_CreateGameObject("MeshGameObject", "crowd",
                                       r3dPoint3D(float(i), 0.0f, 0.0f));
        REQUIRE(objs[i] != nullptr);
        ids[i] = objs[i]->ID;
    }
    CHECK_EQ(GameWorld().GetNumObjects(), COUNT);

    // Delete every other one.
    for (int i = 0; i < COUNT; i += 2)
        objs[i]->setActiveFlag(0);

    for (int f = 0; f < 4; ++f)
        tick();

    CHECK_EQ(GameWorld().GetNumObjects(), COUNT / 2);
    for (int i = 0; i < COUNT; ++i) {
        if (i % 2 == 0)
            CHECK(GameWorld().GetObject(ids[i]) == nullptr);
        else
            CHECK(GameWorld().GetObject(ids[i]) != nullptr);
    }
}

WARZ_TEST(objectmanager, object_ids_are_not_reused_within_a_session)
{
    // A stale gobjid_t that silently starts resolving to a DIFFERENT object is worse
    // than one that resolves to nothing: the first is a wrong answer, the second is a
    // null check that already exists everywhere.
    WorldFixture w;

    GameObject* first = srv_CreateGameObject("MeshGameObject", "first",
                                             r3dPoint3D(0.0f, 0.0f, 0.0f));
    REQUIRE(first != nullptr);
    const gobjid_t first_id = first->ID;

    first->setActiveFlag(0);
    for (int f = 0; f < 4; ++f)
        tick();
    REQUIRE(GameWorld().GetObject(first_id) == nullptr);

    GameObject* second = srv_CreateGameObject("MeshGameObject", "second",
                                              r3dPoint3D(0.0f, 0.0f, 0.0f));
    REQUIRE(second != nullptr);
    CHECK_NE(second->ID, first_id);
}
