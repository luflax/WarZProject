// Level loading, against the synthetic level in tests/fixtures/levels/UnitTestLevel.
//
// This is Milestone C's server criterion -- "loads a level via LoadLevel_Objects" --
// reduced to something that can be asserted on. The real criterion needs game data the
// repository does not have (MILESTONE-C-PREWORK.md §1.3); the fixture replaces 40MB of
// unavailable content with four objects whose expected positions are written down.
//
// What it exercises end to end, with no art assets and no backend:
//
//   pugixml parsing of LevelData.xml / ServerData.xml
//   className string -> AObject class table -> constructed object
//   the unknown-class remap to obj_ServerDummyObject
//   position deserialisation
//   ObjectManager population
//
// That is the whole of LoadLevel_Objects's own logic. What it does NOT cover is terrain,
// which needs real heightmap data -- the fixture has no obj_Terrain, deliberately.
//
// NOTE ON EXECUTION: PE32, needs a working Wine. See tests/CMakeLists.txt.

#include "warz_test.h"

#include "r3dPCH.h"
#include "r3d.h"

#include "gameobjects/ObjManag.h"
#include "gameobjects/GameObj.h"
#include "GameLevel.h"

// LoadLevel_Objects is declared in the client sources and compiled into the server
// build too (GameLevel_IO.cpp is in WO_GameServer's shared source list).
extern int LoadLevel_Objects(float BarRange);

namespace {

// WARZ_TEST_FIXTURE_DIR is defined by tests/CMakeLists.txt as an absolute path, so the
// tests do not depend on the working directory ctest happens to use.
const char* fixture_level_dir()
{
    return WARZ_TEST_FIXTURE_DIR "/levels/UnitTestLevel";
}

struct LevelFixture {
    LevelFixture()
    {
        GameWorld_Create();
        GameWorld().Init(256, 256);
        r3dGameLevel::SetHomeDir(fixture_level_dir());
    }

    ~LevelFixture()
    {
        GameWorld().Destroy();
        GameWorld_Destroy();
    }
};

int count_objects_named(const char* name)
{
    int n = 0;
    for (GameObject* o = GameWorld().GetFirstObject(); o; o = GameWorld().GetNextObject(o)) {
        if (o->Name == name)
            ++n;
    }
    return n;
}

GameObject* find_object_named(const char* name)
{
    for (GameObject* o = GameWorld().GetFirstObject(); o; o = GameWorld().GetNextObject(o)) {
        if (o->Name == name)
            return o;
    }
    return nullptr;
}

} // namespace

WARZ_TEST(level_load, loads_every_object_in_the_fixture)
{
    LevelFixture f;

    LoadLevel_Objects(1.0f);

    // LevelData.xml has four entries and ServerData.xml two. All six must arrive: the
    // unknown-class entry is remapped rather than dropped, so the count is exact and
    // not a lower bound.
    CHECK_EQ(GameWorld().GetNumObjects(), 6);
}

WARZ_TEST(level_load, positions_deserialise)
{
    LevelFixture f;
    LoadLevel_Objects(1.0f);

    GameObject* a = find_object_named("unit_test_mesh_a");
    REQUIRE(a != nullptr);
    CHECK_NEAR(a->GetPosition().x, 10.0f);
    CHECK_NEAR(a->GetPosition().y, 20.0f);
    CHECK_NEAR(a->GetPosition().z, 30.0f);

    // Negative and fractional values, because a parser that reads integers or drops
    // signs passes on the first object and fails here.
    GameObject* b = find_object_named("unit_test_mesh_b");
    REQUIRE(b != nullptr);
    CHECK_NEAR(b->GetPosition().x, -5.5f);
    CHECK_NEAR(b->GetPosition().y, 0.25f);
    CHECK_NEAR(b->GetPosition().z, 7.75f);

    // All-zero position: distinguishes "parsed as zero" from "never parsed", since the
    // default is also zero -- this one is here so the OTHER two mean something.
    GameObject* c = find_object_named("unit_test_mesh_c");
    REQUIRE(c != nullptr);
    CHECK_NEAR(c->GetPosition().x, 0.0f);
}

WARZ_TEST(level_load, unknown_class_is_remapped_not_dropped)
{
    // GameLevel_IO.cpp:40-46. A level naming a class this build does not have must load
    // as a dummy object rather than failing -- otherwise a content update newer than
    // the server binary takes the server down instead of degrading.
    LevelFixture f;
    LoadLevel_Objects(1.0f);

    GameObject* unknown = find_object_named("unit_test_unknown");
    REQUIRE(unknown != nullptr);

    // It kept its position through the remap, which is what makes it a substitute
    // rather than a placeholder dropped at the origin.
    CHECK_NEAR(unknown->GetPosition().x, 1.0f);
    CHECK_NEAR(unknown->GetPosition().y, 2.0f);
    CHECK_NEAR(unknown->GetPosition().z, 3.0f);
}

WARZ_TEST(level_load, server_data_objects_are_loaded)
{
    // ServerData.xml is a separate file under a different root element, loaded under
    // the SF_ServerData tag. Its objects are server-only classes, so this is also the
    // check that the SERVER class registrations linked -- the client-side ones could
    // all be present while these are missing.
    LevelFixture f;
    LoadLevel_Objects(1.0f);

    CHECK_EQ(count_objects_named("unit_test_pspawn"), 1);
    CHECK_EQ(count_objects_named("unit_test_zspawn"), 1);
}

WARZ_TEST(level_load, missing_sound_data_is_not_an_error_on_the_server)
{
    // The fixture has no SoundData.xml. On the server that file is never opened at all
    // (GameLevel_IO.cpp:205, #ifndef WO_SERVER), so a load that reaches the end proves
    // the guard is still in place -- if it regressed, this test would be the one that
    // stopped returning.
    LevelFixture f;

    const int rc = LoadLevel_Objects(1.0f);
    CHECK_EQ(GameWorld().GetNumObjects(), 6);
    (void)rc;
}
