#!/usr/bin/env python
"""
The script builds ngs.framework for iOS.
The built framework is universal, it can be used to build app and run it on either iOS simulator or real device.

Usage:
    ./build_framework.py <outputdir>

By cmake conventions the output dir should not be a subdirectory of NextGIS store and visualisation support library source tree.

Script will create <outputdir>, if it's missing, and a few its subdirectories:

    <outputdir>
        build/
            iPhoneOS-*/
               [cmake-generated build tree for an iOS device target]
            iPhoneSimulator-*/
               [cmake-generated build tree for iOS simulator]
        ngs.framework/
            [the framework content]

The script should handle minor ngs updates efficiently
- it does not recompile the library from scratch each time.
However, ngs.framework directory is erased and recreated on each run.
"""

from __future__ import print_function
import glob, re, os, os.path, shutil, string, sys, argparse, traceback
import multiprocessing
from subprocess import check_call, check_output, CalledProcessError

ext = 'dylib'

def execute(cmd, cwd = None):
    print("Executing: %s in %s" % (cmd, cwd), file=sys.stderr)
    retcode = check_call(cmd, cwd = cwd)
    if retcode != 0:
        raise Exception("Child returned:", retcode)

def getXCodeMajor():
    ret = check_output(["xcodebuild", "-version"])
    m = re.match(r'XCode\s+(\d)\..*', ret, flags=re.IGNORECASE)
    if m:
        return int(m.group(1))
    return 0

class Builder:
    def __init__(self, ngs, targets):
        self.ngs = os.path.abspath(ngs)
        self.targets = targets

    def getBD(self, parent, t):
        res = os.path.join(parent, '%s-%s' % t)
        if not os.path.isdir(res):
            os.makedirs(res)
        return os.path.abspath(res)

    def _build(self, outdir):
        outdir = os.path.abspath(outdir)
        if not os.path.isdir(outdir):
            os.makedirs(outdir)
        dirs = []

        xcode_ver = getXCodeMajor()

        for t in self.targets:
            mainBD = self.getBD(outdir, t)
            dirs.append(mainBD)
            cmake_flags = []
            if xcode_ver >= 100 and t[1] == 'iPhoneOS': # 7
                cmake_flags.append("-DCMAKE_C_FLAGS=-fembed-bitcode")
                cmake_flags.append("-DCMAKE_CXX_FLAGS=-fembed-bitcode")
            self.buildOne(t[0], t[1], mainBD, cmake_flags)
            self.mergeLibs(mainBD)
        self.makeFramework(outdir, dirs)

    def build(self, outdir):
        try:
            self._build(outdir)
        except Exception as e:
            print("="*60, file=sys.stderr)
            print("ERROR: %s" % e, file=sys.stderr)
            print("="*60, file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
            sys.exit(1)

    def getCMakeArgs(self, arch, target):
        args = [
            "cmake",
            "-GUnix Makefiles",
            "-DBUILD_TARGET_PLATFORM=IOS",
            "-DCMAKE_INSTALL_PREFIX=install",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DIOS_ARCH=" + arch,
        ]

        if target == 'iPhoneSimulator':
            args.append("-DIOS_PLATFORM=SIMULATOR")
        else:
            args.append("-DIOS_PLATFORM=OS")

        if ext == 'dylib':
            args.append("-DBUILD_SHARED_LIBS=ON")
        return args

    def getBuildCommand(self, arch, target):
        maxCount = multiprocessing.cpu_count()
        # if maxCount > 2:
        #     maxCount = 2
        buildcmd = [
            "cmake",
            "--build",
            ".",
            "--config",
            "release",
            "--",
            "-j",
            str(maxCount)
        ]
        return buildcmd


    def getInstallCommand(self, arch, target):
        buildcmd = [
            "cmake",
            "--build",
            ".",
            "--target",
            "install",
        ]
        return buildcmd

    def getInfoPlist(self, builddirs):
        return os.path.join(builddirs[0], "ios", "Info.plist")

    def buildOne(self, arch, target, builddir, cmakeargs = []):
        # Run cmake
        cmakecmd = self.getCMakeArgs(arch, target)
        if arch.startswith("armv") or arch.startswith("arm64"):
            cmakecmd.append("-DENABLE_NEON=ON")
        cmakecmd.append(self.ngs)
        cmakecmd.extend(cmakeargs)
        execute(cmakecmd, cwd = builddir)
        # Clean and build
        clean_dir = os.path.join(builddir, "install")
        if os.path.isdir(clean_dir):
            shutil.rmtree(clean_dir)
        buildcmd = self.getBuildCommand(arch, target)
        execute(buildcmd, cwd = builddir)
        execute(["cmake", "--build", ".", "--target", "install"], cwd = builddir)

    def mergeLibs(self, builddir):
        res = os.path.join(builddir, "install", "lib", "libngs_merged.a")
        libs = glob.glob(os.path.join(builddir, "install", "lib", "*.a"))
        print("Merging libraries:\n\t%s" % "\n\t".join(libs), file=sys.stderr)
        execute(["libtool", "-static", "-o", res] + libs)

    def makeFramework(self, outdir, builddirs):
        name = "ngstore"
        libname = "libngs_merged." + ext

        # set the current dir to the dst root
        framework_dir = os.path.join(outdir, "%s.framework" % name)
        if os.path.isdir(framework_dir):
            shutil.rmtree(framework_dir)
        os.makedirs(framework_dir)

        dstdir = os.path.join(framework_dir, "Versions", "A")

        # copy headers from one of build folders
        shutil.copytree(os.path.join(builddirs[0], "install", "include", "ngstore"), os.path.join(dstdir, "Headers"))

        # make universal static lib
        libs = [os.path.join(d, "install", "lib", libname) for d in builddirs]
        lipocmd = ["lipo", "-create"]
        lipocmd.extend(libs)
        lipocmd.extend(["-o", os.path.join(dstdir, name)])
        print("Creating universal library from:\n\t%s" % "\n\t".join(libs), file=sys.stderr)
        execute(lipocmd)

        # copy Info.plist
        resdir = os.path.join(dstdir, "Resources")
        os.makedirs(resdir)
        shutil.copyfile(self.getInfoPlist(builddirs), os.path.join(resdir, "Info.plist"))

        moddir = os.path.join(dstdir, "Modules")
        os.makedirs(moddir)
        f = open(os.path.join(moddir, 'module.modulemap'), 'w')
        f.write('''framework module ngstore {
  header "api.h"
  header "codes.h"
  header "common.h"
}''')

  # umbrella header "api.h"
  #
  # export *
  # module * { export * }
  #
        # make symbolic links
        links = [
            (["A"], ["Versions", "Current"]),
            (["Versions", "Current", "Headers"], ["Headers"]),
            (["Versions", "Current", "Resources"], ["Resources"]),
            (["Versions", "Current", "Modules"], ["Modules"]),
            (["Versions", "Current", name], [name])
        ]
        for l in links:
            s = os.path.join(*l[0])
            d = os.path.join(framework_dir, *l[1])
            os.symlink(s, d)

if __name__ == "__main__":
    folder = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), "../.."))
    parser = argparse.ArgumentParser(description='The script builds ngs.framework for iOS.')
    parser.add_argument('out', metavar='OUTDIR', help='folder to put built framework')
    parser.add_argument('--repo', metavar='DIR', default=folder, help='folder with ngs repository (default is "../.." relative to script location)')
    args = parser.parse_args()

    b = Builder(args.repo,
        [
            ("armv7", "iPhoneOS"),
            ("armv7s", "iPhoneOS"),
            ("arm64", "iPhoneOS"),
            ("i386", "iPhoneSimulator"),
            ("x86_64", "iPhoneSimulator"),
        ])
    b.build(args.out)
