// Wire layout of the network protocol. static_assert only -- compiling this file IS
// the test, so it runs in every environment, including the ones where Wine is missing
// and nothing PE32 can be executed.
//
// WHY THIS IS THE FIRST TEST WORTH HAVING
//
// P2PMessages.h declares ~156 `#pragma pack(1)` POD structs and casts raw RakNet
// payloads to them. The client and the server are separate binaries; for the protocol
// to work, both must agree byte-for-byte on every one. This codebase was laid out by
// MSVC 2008 in 2013 and is now laid out by i686-w64-mingw32-g++ 13, and the two do not
// have to agree -- `#pragma pack` is not in the standard, and MSVC and GCC differ over
// bitfields, over empty base classes, and over whether a pragma survives a template
// instantiation.
//
// A disagreement here does not crash. The server reads a field from the wrong offset
// and carries on: a position becomes garbage, a weapon id lands in a quantity, and the
// symptom surfaces a hundred files away from the cause. That is the single worst class
// of bug this port can ship, and it costs one afternoon to fence off.
//
// WHAT THESE ASSERTIONS DO AND DO NOT PROVE
//
//   They DO prove the layout has not drifted since the baseline was frozen -- a pack
//   pragma that stops applying, a base class that grows a vptr, an enum whose width
//   changes, a field silently reordered.
//
//   They DO prove, via the no-padding checks below, that pack(1) is genuinely in
//   effect. That property is compiler-independent, so it is real evidence about MSVC
//   compatibility rather than a self-consistency check.
//
//   They do NOT prove this layout matches what the 2013 MSVC build produced. Nothing
//   available here can: that would need the original compiler, or a captured packet
//   trace from a running server. Until one of those exists, the no-padding property is
//   the strongest claim on offer, and it is a strong one -- for a struct of scalars
//   with no padding, layout is fully determined by the member order and their widths.
//
// Regenerate the baseline with tools/gen_packet_layout.py, and only alongside a
// deliberate packet change with a P2PNET_VERSION bump in the same commit.

#include "r3dPCH.h"
#include "r3d.h"
#include "multiplayer/P2PMessages.h"

#include <cstddef>
#include <type_traits>

// ---------------------------------------------------------------------------
// The frozen baseline
// ---------------------------------------------------------------------------

#define WARZ_PACKET_SIZE(type, bytes)                                                 \
    static_assert(sizeof(type) == (bytes),                                            \
                  #type " changed size -- the wire format moved. If that was "        \
                  "intended, rerun tools/gen_packet_layout.py and bump "              \
                  "P2PNET_VERSION.");

#include "packet_sizes.inc"

#undef WARZ_PACKET_SIZE

// ---------------------------------------------------------------------------
// Properties that hold regardless of the baseline
//
// These are the compiler-independent half, and the reason this file is more than a
// self-consistency check.
// ---------------------------------------------------------------------------

// NO VTABLES. This is the assertion that matters most after the sizes themselves: the
// receive path casts a raw RakNet byte buffer straight to these types, so a vptr would
// occupy the first four bytes, shift every field by four, and make each packet read its
// neighbour's data. Nothing about that is a compile error -- one `virtual` added to
// r3dNetPacketHeader or to any struct it reaches would do it silently.
//
// It is also not hypothetical: r3dNetCallback, declared twelve lines above
// r3dNetPacketHeader in the same header, IS polymorphic.
#define WARZ_PACKET_SIZE(type, bytes)                                                 \
    static_assert(!std::is_polymorphic_v<type>,                                       \
                  #type " gained a vtable -- every field after it is now misaligned "  \
                  "against the wire format");                                         \
    static_assert(std::is_trivially_destructible_v<type>,                             \
                  #type " must be trivially destructible -- packets are cast from a "  \
                  "socket buffer and never destroyed through this type");

#include "packet_sizes.inc"

#undef WARZ_PACKET_SIZE

// NOT asserted here, and deliberately so -- two properties one would reach for first
// are false of this design, and asserting them would only teach people to delete
// assertions:
//
//   is_standard_layout   is false for every packet. DefaultPacket carries FromID and
//                        the derived struct carries its own fields, and a hierarchy
//                        with data members in more than one class is not standard
//                        layout by rule. The layout is still fixed and well-defined;
//                        it is only the trait that does not apply.
//
//   is_trivially_copyable / copy-assignable is false because r3dNetPacketHeader
//                        declares `const BYTE RakNetPacketID` and `const BYTE EventID`
//                        (r3dNetwork.h:68-69). The const is load-bearing: it is what
//                        stops a packet's own type tag from being overwritten by an
//                        assignment. Deleting copy-assignment is the intended cost.
//
// The const-ness itself is worth pinning, since removing it to "fix" the traits above
// would quietly re-enable exactly the bug it prevents.
static_assert(std::is_const_v<decltype(r3dNetPacketHeader::RakNetPacketID)>,
              "RakNetPacketID must stay const -- it is the RakNet type tag and must not "
              "be assignable");
static_assert(std::is_const_v<decltype(r3dNetPacketHeader::EventID)>,
              "EventID must stay const -- it is the packet type tag and must not be "
              "assignable");

// ---------------------------------------------------------------------------
// pack(1) is genuinely in effect
//
// Spot-checked by hand against structs whose members are chosen to expose padding: any
// struct mixing a 1-byte and a 4-byte member has natural alignment that would insert
// padding, so if sizeof() equals the exact sum of the member widths, the pragma applied.
//
// These are hand-written rather than generated on purpose. The generated table above
// records whatever the compiler did; these record what the protocol REQUIRES, which is
// the difference between a regression test and a specification.
// ---------------------------------------------------------------------------

// PKT_C2C_MoveRel_s -- the movement packet, sent continuously for every visible player.
// Its whole reason for existing is that turn and bend angles are quantised to single
// bytes (CNetCellMover, multiplayer/NetCellMover.h). One byte of padding here would be
// a measurable bandwidth regression across 512 players as well as a layout break.
static_assert(sizeof(PKT_C2C_MoveRel_s) == 11,
              "MoveRel must stay 11 bytes: 4 (mixin) + 3 x 2 (packed shorts) + 1 (flags). "
              "Anything larger means pack(1) stopped applying to it.");

// PKT_S2C_SetPlayerVitals_s -- 4-byte mixin plus five single bytes. Natural alignment
// would round this to 12; pack(1) keeps it at 9.
static_assert(sizeof(PKT_S2C_SetPlayerVitals_s) == 9,
              "SetPlayerVitals must stay 9 bytes -- with natural alignment it would be 12, "
              "which is what a dropped pack(1) looks like.");

// The header every packet derives from: two const BYTEs of type tag
// (r3dNetPacketHeader) plus a 2-byte FromID (DefaultPacket). Four bytes exactly, and
// every field in every packet in the protocol is positioned relative to it -- so if
// this number moves, all 154 move with it, and this assertion is the one that says so
// in one line instead of 154.
static_assert(sizeof(r3dNetPacketHeader) == 2, "packet type tag must be 2 bytes");
static_assert(sizeof(DefaultPacket) == 4, "packet header must be 2-byte tag + 2-byte FromID");
static_assert(sizeof(PKT_C2C_PacketBarrier_s) == sizeof(DefaultPacket),
              "an empty packet must be exactly its header -- no padding, no vptr");

// ---------------------------------------------------------------------------
// The protocol version constant
//
// P2PNET_VERSION (P2PMessages.h:25) is the client/server compatibility gate, and it is
// built from three sub-version constants. Freezing it means a packet change that forgot
// to bump it cannot pass review silently -- the reviewer is shown a diff of this line.
// ---------------------------------------------------------------------------

static_assert(P2PNET_VERSION == (0x000001ce + GBWEAPINFO_VERSION + GBGAMEINFO_VERSION
                                 + GAMEPLAYPARAM_VERSION),
              "P2PNET_VERSION is no longer the sum of its parts");

// Sanity on the id space: players occupy net ids [1, MAX_NUM_PLAYERS] and spawned
// objects start at NETID_OBJECTS_START. If the player range ever grew past the object
// base, player and object ids would alias and the two would overwrite each other.
static_assert(NETID_PLAYERS_START + MAX_NUM_PLAYERS <= NETID_OBJECTS_START,
              "player net-id range overlaps the object net-id range");
