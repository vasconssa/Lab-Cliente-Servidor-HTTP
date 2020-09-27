matches = os.matchfiles(TESTDIR .. "/*.c")

for i = 1, #matches do
    name = path.getbasename(matches[i])

    project(name)
    system "Linux"
    architecture "x86_64"

    kind "ConsoleAPP"
    language "C"

    includedirs {
        path.join(DIR, "include"),
        path.join(DIR, "3rdparty"),
    }

    links {
        --"sx",
        "common",
        "m",
        "dl",
        "pthread",
    }

    files {
        matches[i],
    }
end

