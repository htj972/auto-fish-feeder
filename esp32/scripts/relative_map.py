"""Keep the linker map path ASCII when the project lives in a Chinese directory."""
import os

Import("env")

map_path = os.path.relpath(
    os.path.join(env.subst("$BUILD_DIR"), env.subst("$PROGNAME") + ".map"),
    env.subst("$PROJECT_DIR"),
).replace("\\", "/")
env.Replace(
    LINKFLAGS=[
        '-Wl,-Map="' + map_path + '"' if str(flag).startswith("-Wl,-Map=") else flag
        for flag in env.get("LINKFLAGS", [])
    ]
)
