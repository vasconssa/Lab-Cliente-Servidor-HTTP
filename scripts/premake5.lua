require "export-compile-commands"

DIR = path.getabsolute("..")
print(DIR)

THIRDPARTYDIR = path.join(DIR, "3rdparty")
BUILDDIR = path.join(DIR, "build")
TESTDIR = path.join(DIR, "tests")

workspace "Gabibits"
    configurations { "Debug", "Release" }
    platforms { "Android-Arm", "Win32", "Win64", "Linux32", "Linux64" }
    toolset "clang"

include("toolchain.lua")

project "common"
    kind "StaticLib"
    language "C"
    includedirs {
        path.join(DIR, "include"),
        path.join(DIR, "3rdparty"),
    }
    links {
        "m",
        "pthread",
    }
    
    files {
        path.join(DIR, "src/http.c"),
    }

--project "sx"
    --kind "StaticLib"
    --language "C"

    --includedirs {
        --path.join(DIR, "include"),
        --path.join(DIR, "3rdparty"),
    --}

    --links {
        --"m",
        --"pthread",
    --}

    --files {
        --path.join(DIR, "src/sx/*.c"),
        ----DIR .. "/src/sx/*.c",
    --}

    --filter "platforms:Linux64"
    --system "Linux"
    --architecture "x86_64"

    --files { 
        --DIR .. "/src/sx/asm/make_x86_64_sysv_elf_gas.S",
        --DIR .. "/src/sx/asm/jump_x86_64_sysv_elf_gas.S",
        --DIR .. "/src/sx/asm/ontop_x86_64_sysv_elf_gas.S",
    --}
    --filter "architecture:x86"
    --system "Linux"
    --filter {}


project "server"
    kind "ConsoleApp"
    language "C"

    includedirs {
        path.join(DIR, "include"),
        path.join(DIR, "3rdparty"),
    }

    links {
        --"sx",
    }
    
    prebuildcommands('{MKDIR} %{cfg.buildtarget.directory}/www')


    filter "platforms:Linux64"
    system "Linux"
    architecture "x86_64"
    symbols "on"

    links {
        "common",
        "pthread",
        "m",
    }
    
    filter {}

    files {
        path.join(DIR, "src/**.c"),
    }

    removefiles {
        path.join(DIR, "src/sx/*.c"),
    }


include "tests.lua"
