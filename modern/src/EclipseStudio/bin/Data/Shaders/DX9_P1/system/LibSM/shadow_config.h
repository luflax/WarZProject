// [PORT] Placeholder for a SHADER header that ships in bin/, which .gitignore excludes
// from this repository. It is authored as HLSL and #included by both the shadow shaders
// and three C++ translation units (RenderDeffered.cpp, LevelEditor.cpp, tree.cpp) so the
// two sides agree on shadow-map settings.
//
// The C++ side turns out not to need it: every identifier those three files reference
// from the shadow-map configuration is already defined in the C++ headers --
// SHADOWC_PIXELDIAMETER in Eternity/Include/r3dMat.h:400 and the SHADOWACCUM_LIGHT_*
// enumerators in RENDERING/Deffered/RenderDeffered.h:366. All three compile clean
// against this empty file.
//
// So this exists only to satisfy the #include. When the shader tree is restored, drop
// the real header in at this path and delete this note -- nothing else has to change.
//
// Two copies exist because the three call sites spell the relative path differently
// (../../../bin/... from RENDERING/Deffered/, ../../bin/... from Editors/ and
// ObjectsCode/WORLD/), which resolves to two different directories under a compiler
// that searches relative to the including file. The Sources/ copy forwards here.

#pragma once
