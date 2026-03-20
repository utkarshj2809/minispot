import os
import sys


CD = "cd ."

if "novs" not in sys.argv:
    # might need to change this
    VS_INIT = '"C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat"'
else:
    VS_INIT = "echo Not initializing VS environment."

class CLCommand:
    cur: str

    def __init__(self):
        self.cur = "cl "

    def with_resources(self, *res: list[str]):
        self.cur += " ".join(res) + " "
        return self

    def with_standard(self, std: str):
        self.cur += f"/std:{std} "
        return self

    def with_better_exception_handling(self):
        self.cur += "/EHsc "
        return self

    def with_pps_macro(self, mcr: str):
        self.cur += f"/D{mcr} "
        return self

    def with_includes(self, *incs: list[str]):
        self.cur += "/I "
        for i in incs:
            if "\\" in i:
                self.cur += f"{i} "
            else:
                self.cur += f"{i}.lib "
        self.cur += " "
        return self
    
    def with_outputdir(self, d: str):
        self.cur += f"/Fe:{d} "
        return self

    def build(self):
        return self.cur[:-1]

# "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"


# what's Make?
CL = (
    CLCommand()
    .with_resources("main.cpp", "resources.res")
    .with_outputdir("./dist/")
    .with_standard("c++20")
    .with_better_exception_handling()
    .with_pps_macro("WEBVIEW2_STATIC_LIB")
    .with_includes(
        "wv2\\build\\native\\include",
        "wv2\\build\\native\\x64\\webview2loaderstatic.lib",
        "user32",
        "gdi32",
        "ole32",
        "shell32",
        "advapi32",
        "wevtapi",
        "Shlwapi",
        "dwmapi",
        "winhttp",
        "crypt32"
    )
)

# resources compiling
RES = "rc resources.rc"


# put one file into the output stream
# idk if i need a function for this but whatevs
def webcompile_one(fp, out, tag=None):
    if tag:
        out.write(f"<{tag}>")

    with open(fp, "r") as f:
        while True:
            chunk = f.read(1067008)
            if not chunk:
                break
            out.write(chunk)

    if tag:
        out.write(f"</{tag}>")


def compile_web():
    output = open("./web_compiled.html", "w")
    webcompile_one("web/dist/index.html", output)

    output.close()


def main():
    os.makedirs("dist", exist_ok=True)
    compile_web()
    # deprecation is for nerds
    os.system(" && ".join([CD, VS_INIT, RES, CL.build()]))


if __name__ == "__main__":
    main()
