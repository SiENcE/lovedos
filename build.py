#!/usr/bin/env python
from __future__ import print_function
import os, sys, shutil, re, textwrap

COMPILER    = os.environ.get("DJGPP_CC", "i586-pc-msdosdjgpp-gcc")
SRC_DIR     = "src"
BIN_DIR     = "bin"
BIN_NAME    = "love.exe"
EMBED_DIR   = "src/embed"
TEMPSRC_DIR = ".tempsrc"

# -march=i386 keeps the executable free of 486+ instructions (such as bswap,
# which GCC emits when targeting the default Pentium) so it runs on 386s
CFLAGS    = [ "-O2", "-Wall", "-s", "-Wno-misleading-indentation",
              "-march=i386", "-mtune=i486" ]
DLIBS     = [ "m" ]
DEFINES   = [ "DMT_ABORT_NULL", "LUA_COMPAT_ALL" ]
INCLUDES  = [ "src", TEMPSRC_DIR ]


def fmt(fmt, var):
  for k in var:
    fmt = fmt.replace("{%s}" % str(k), var[k])
  return fmt


def listdir(path):
  return [os.path.join(dp, f) for dp, dn, fn in os.walk(path) for f in fn]


def make_c_include(name, data):
    name = re.sub("[^a-z0-9]", "_", name.lower())
    res = "static const char " + name + "[] = {"
    for c in bytearray(data):
        res += str(c) + ", "
    res = res.rstrip(", ") + "};"
    return name, textwrap.fill(res, width=79)


def main():
  os.chdir(sys.path[0])

  if not os.path.exists(BIN_DIR):
    os.makedirs(BIN_DIR)

  if not os.path.exists(TEMPSRC_DIR):
    os.makedirs(TEMPSRC_DIR)

  embedded_files = listdir(EMBED_DIR)

  for filename in embedded_files:
    name = os.path.basename(filename)
    name, text = make_c_include(name, open(filename, "rb").read())
    open("%s/%s.h" % (TEMPSRC_DIR, name), "w").write(text)

  cfiles = filter(lambda x:x.endswith((".c", ".C")), listdir(SRC_DIR))

  cmd = fmt(
    "{compiler} {flags} {defines} {includes} -o {outfile} {srcfiles} {libs} {argv}",
    {
      "compiler"  : COMPILER,
      "flags"     : " ".join(CFLAGS),
      "defines"   : " ".join(map(lambda x: "-D " + x, DEFINES)),
      "includes"  : " ".join(map(lambda x: "-I " + x, INCLUDES)),
      "outfile"   : BIN_DIR + "/" + BIN_NAME,
      "srcfiles"  : " ".join(cfiles),
      "libs"      : " ".join(map(lambda x: "-l" + x, DLIBS)),
      "argv"      : " ".join(sys.argv[1:])
    })

  print("compiling...")
  res = os.system(cmd)

  print("deleting temporary files...")
  if os.path.exists(TEMPSRC_DIR):
      shutil.rmtree(TEMPSRC_DIR)

  print("done" + (" with errors" if res else ""))
  return 1 if res else 0



if __name__ == "__main__":
  sys.exit(main())
