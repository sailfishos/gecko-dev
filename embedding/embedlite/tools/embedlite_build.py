#!/usr/bin/env python3
# -*- encoding: utf8 -*-

# Copyright (c) 2020 Open Mobile Platform LLC.

import sys, os, stat, time, getopt, pickle
from functools import reduce

# ============================================================================
# LOGGING
# ============================================================================

# syslog(3) compatible logging levels
LOG_EMERG   = 0
LOG_ALERT   = 1
LOG_CRIT    = 2
LOG_ERR     = 3
LOG_WARNING = 4
LOG_NOTICE  = 5
LOG_INFO    = 6
LOG_DEBUG   = 7

# Level indicator prefixes to use for logging
LOG_PREFIX = {
LOG_EMERG   : "X",
LOG_ALERT   : "A",
LOG_CRIT    : "C",
LOG_ERR     : "E",
LOG_WARNING : "W",
LOG_NOTICE  : "N",
LOG_INFO    : "I",
LOG_DEBUG   : "D",
}

# Current verbosity level
LOG_LEVEL = LOG_NOTICE

# Increase logging verbosity
def log_more_verbose():
    global LOG_LEVEL
    LOG_LEVEL = min(LOG_LEVEL + 1, LOG_DEBUG)

# Decrease logging verbosity
def log_less_verbose():
    global LOG_LEVEL
    LOG_LEVEL = max(LOG_LEVEL - 1, LOG_CRIT)

# Logging predicate
def log_p(lev):
    return lev <= LOG_LEVEL

# Emit log entry
def log_emit(lev, msg):
    if log_p(lev):
        sys.stderr.write("%s: %s\n" % (LOG_PREFIX[lev], msg))

# Logging level specific helpers
def log_crit(msg):    log_emit(LOG_CRIT,    msg)
def log_err(msg):     log_emit(LOG_ERR,     msg)
def log_warning(msg): log_emit(LOG_WARNING, msg)
def log_notice(msg):  log_emit(LOG_NOTICE,  msg)
def log_info(msg):    log_emit(LOG_INFO,    msg)
def log_debug(msg):   log_emit(LOG_DEBUG,   msg)

# ============================================================================
# Config / Options
# ============================================================================

# Relative path to objects directory
TARGET_BASE = "obj-build-mer-qt-xr"

# Basename for configuration / temp files
CONFIG_FILE_PREFIX = "embedding/embedlite/tools/embedlite_build"

# Construct configuration file path
def config_path(name):
    if '.' in name:
        ext = ''
    else:
        ext = ".conf"
    return "%s.%s%s" % (CONFIG_FILE_PREFIX, name, ext)

# Read whitespace limited list of entries from config file
def read_list(name):
    path = config_path(name)
    if not os.path.exists(path):
        log_warning("%s: does not exist" % path)
        return tuple()
    return open(path).read().split(None)

# Persistent directory scanner data in pickle dump format
FSCACHE_FILE = config_path("fscache.db")

# Script file used for executing actions
ACTIONS_SCRIPT = config_path("actions.sh")

# Are we running in sdk chroot or scratchbox target
PROBED_WITHIN_SBOX_TARGET = False

# If not already in sbox target, which target to enter (None = use default)
OPTION_USE_SBOX_TARGET = None

# Forced mtime update for regular files
OPTION_PARANOID_SCANNING = False

# Use persistent fsstate cache
OPTION_PERSISTENT_FSSTATE = True

# Do not actually execute build actions
OPTION_DRY_RUN = False

# ============================================================================
# File type heuristics
# ============================================================================

# ELF object header
TAG_ELF = b'\x7f\x45\x4c\x46'

# AR archive header
TAG_LIB = b'\x21\x3c\x61\x72'

# Source file extensions (read-only)
EXT_SOURCE  = dict.fromkeys(read_list("source_extensions"))

# Object/library file extensions (read-only)
EXT_OBJECT  = dict.fromkeys(read_list("object_extensions"))

# Ignored file extensions (read-only)
EXT_IGNORE  = dict.fromkeys(read_list("ignored_extensions"))

# Files that should be ignored
FT_IGNORE = 0

# C, C++, etc source files
FT_SOURCE = 1

# Elf object files / AR arhives = target files
FT_OBJECT = 2

# Filename to filetype mapping cache
BASENAME_FILETYPE_LUT = {}

# Evaluate file type based on extension / probe for ELF/AR file
#
# First try to evaluate typen based on extension:
# - applies for all source files
# - those object files that have distinct exension
# - as many files that can be ignored as possible
#
# Files that slip through extension logic:
# - take a peek at the first four bytes of file content
# - ELF objects/binaries and AR archives are "objects"
# - the rest is classified as "ignored"
# - if the file has extension, other files with the same
#   extension will be mapped to the same type (during runtime
#   only, these decisions are not stored persistently)

def filetype_guess(path, name):
    # ignore "hidden" files without further analysis
    if name.startswith("."):
        return FT_IGNORE

    ext = os.path.splitext(name)[1].lower()

    # handle assumptions about extension
    if ext:
        if ext in EXT_SOURCE: return FT_SOURCE
        if ext in EXT_IGNORE: return FT_IGNORE
        if ext in EXT_OBJECT: return FT_OBJECT

    # files with no or unknown extension
    # -> check if they have ELF or ar header
    path = os.path.join(path, name)
    head = open(path, "rb").read(4)
    log_debug("probing: head= %s @ path=%s" % (repr(head), path))
    if head == TAG_ELF or head == TAG_LIB:
        if ext:
            EXT_OBJECT[ext] = None
        return FT_OBJECT

    if ext:
        EXT_IGNORE[ext] = None
    return FT_IGNORE

# Evaluate file type based on file basename
#
# Assumes: files that have the same name also have the same type
#
# In practice this avoids repetitive probing of uninteresting files
# without extension e.g. "Makefile".

def filetype(path,name):
    try:
        ft = BASENAME_FILETYPE_LUT[name]
    except:
        BASENAME_FILETYPE_LUT[name] = ft = filetype_guess(path, name)
    return ft

# ============================================================================
# Wrappers for file/directory operations
# ============================================================================

def os_lstat(path):
    try:
        return os.lstat(path)
    except OSError as e:
        log_warning("%s: stat: %s" % (e.filename, e.strerror))
    return None

def os_listdir(path):
    try:
        return os.listdir(path)
    except OSError as e:
        log_warning("%s: listdir: %s" % (e.filename, e.strerror))
    return None

# ============================================================================
# CachedFile
# ============================================================================

class CachedFile:
    def __init__(self, directory, name, mtime_ns):
        self.__directory = directory
        self.__name      = name
        self.__mtime_ns  = mtime_ns
        self.__typeid    = None

    def directory(self):
        return self.__directory

    def name(self):
        return self.__name

    def timestamp(self):
        return self.__mtime_ns

    def __str__(self):
        return self.__name

    def __repr__(self):
        return self.path()

    def filetype(self):
        if self.__typeid == None:
            self.__typeid = filetype(self.directory().path(), self.name())
        return self.__typeid

    def update(self):
        st = os_lstat(self.path())
        if st == None:
            return False

        if not stat.S_ISREG(st.st_mode):
            log_warning("%s: %s" % (path, "is no longer a regular file"))
            return False

        self.__mtime_ns = st.st_mtime_ns
        return True

    def path(self):
        return os.path.join(self.directory().path(), self.name())

# ============================================================================
# CachedDirectory
# ============================================================================

# Pros and cons of caching ...

# With current scanner logic cache initialization (full scan) takes
# about 3 seconds.

# Loading, updating, saving cache takes about 0.5 + 0.3 + 0.5 = 1.3 seconds
# i.e. detecting situations with little/no changes is about twice as fast
# as with no caching.

# The caveat is that the saving comes from skipping per file stat() calls
# based on mtime of containing directory - which does end up changing
# during normal editing/building activity, but not when something like
# touch foo/bar.cc is done

# And avoiding such false negatives means that also files need to be
# checked, which then makes updating cache slower than straight forward
# complete scan.. sigh.

# For now: use caching and optimistic updates, figure out later if the
# cache update code can be optimized vs if it makes more sense to throw
# away caching altogether

class CachedDirectory:
    def __init__(self, parent, name):
        self.__name     = name
        self.__parent   = parent
        self.__mtime_ns = 0
        self.__dirs     = []
        self.__files    = []

    def name(self):
        return self.__name

    def parent(self):
        return self.__parent

    def mtime_ns(self):
        return self.__mtime_ns

    def dirs(self):
        return self.__dirs

    def files(self):
        return self.__files

    def __str__(self):
        return "%s/" % self.name()

    def __repr__(self):
        return self.path()

    def files_of_type(self, typeid):
        return list(filter(lambda x:x.filetype() == typeid, self.__files))

    def source_files(self):
        return self.files_of_type(FT_SOURCE)

    def object_files(self):
        return self.files_of_type(FT_OBJECT)

    def dump(self, indent=0):
        print("%*sD: %s" % (indent, "", self.name()))
        indent += 4
        for child in self.__files:
            print("%*sF:%s" % (indent, "", child.name()()))
        for child in self.__dirs:
            child.dump(indent)

    def path(self):
        # root node path: (assumed) full path
        # other nodes: path relative to root
        v = [self.name()]
        p = self.parent()
        while p and p.parent():
            v.append(p.name())
            p = p.parent()
        return reduce(os.path.join, reversed(v))

    def locate_dir_by_name(self, name):
        # scanner keeps entries in order -> binary search
        l,h = 0,len(self.__dirs)
        while l < h:
            i = (l + h) // 2
            item = self.__dirs[i]
            if item.name() < name:
                l = i + 1
            elif item.name() > name:
                h = i
            else:
                return item
        return None

    def locate_dir(self, path):
        now = self
        while path:
            if '/' in path:
                pfix,path = path.split("/", 1)
            else:
                pfix,path = path, None
            zen = now.locate_dir_by_name(pfix)
            if not zen:
                log_debug("%s: does not have '%s' child" % (now.path(), pfix))
                return None
            now = zen
        return now

    def update(self):
        path = self.path()

        # Get stats and do sanity check
        st = os_lstat(path)
        if st == None:
            return False

        if not stat.S_ISDIR(st.st_mode):
            log_warning("%s: %s" % (path, "is no longer a directory"))
            return False

        # If mtime has not changed, assume cache is valid and only check
        # subdirs. This will miss file modifications that do not cause
        # changes in containing directory e.g. touch foo/bar.c is not
        # detected - but canonical/sane saving from editor involves temp
        # files and renaming and thus is caught.
        if self.mtime_ns() == st.st_mtime_ns:
            log_debug("%s: %s" % (path, "using cached data"))
            if OPTION_PARANOID_SCANNING:
                self.__files = list(filter(lambda x:x.update(), self.__files))
            self.__dirs = list(filter(lambda x:x.update(), self.__dirs))
            return True

        # Rescan directory. File info is nuked and  constructed from
        # scratch. But merge sort style algorithm is used in order
        # to retain applicable parts of already cached subdirectory
        # info.
        if self.__mtime_ns > 0:
            log_info("%s: %s" % (path, "updating cached data"))
        self.__mtime_ns = st.st_mtime_ns
        self.__files = []
        prev = self.__dirs
        prev.reverse()
        self.__dirs = []

        curr = os_listdir(path)
        if curr == None:
            return False

        for name in sorted(curr):
            if name == ".git":
                continue

            full = os.path.join(path, name)
            st = os_lstat(full)
            if st == None:
                continue

            # regular files are collected to files list
            if stat.S_ISREG(st.st_mode):
                self.__files.append(CachedFile(self, name, st.st_mtime_ns))
                continue

            # sockets, fifos, symlinks, etc are ignored
            if not stat.S_ISDIR(st.st_mode):
                log_debug("%s: %s" % (path, "is ignored"))
                continue

            # discard stale directory entries
            while prev and prev[-1].name() < name:
                item = prev.pop()
                log_debug("%s: %s - dropped" % (path, item.name()))

            # use existing directory entry / create a new one
            if prev and prev[-1].name() == name:
                item = prev.pop()
                log_debug("%s: %s - updated" % (path, item.name()))
            else:
                item = CachedDirectory(self, name)
                log_debug("%s: %s - created" % (path, item.name()))

            # if recursive update succeeds, add to directories list
            if item.update():
                self.__dirs.append(item)

        # discard remaining stale directory entries
        if log_p(LOG_DEBUG):
            while prev:
                item = prev.pop()
                log_debug("%s: %s - dropped" % (path, item.name()))

        return True

# ============================================================================
# FSCACHE
# ============================================================================

def fsstate_save(root):
    if OPTION_PERSISTENT_FSSTATE:
        log_notice("saving fs state")
        pickle.dump(root, open(FSCACHE_FILE, "wb"))

def fsstate_load():
    if OPTION_PERSISTENT_FSSTATE and os.path.exists(FSCACHE_FILE):
        log_notice("loading fs state")
        root = pickle.load(open(FSCACHE_FILE, "rb"))
    else:
        root = CachedDirectory(None, os.path.realpath("."))
    return root

def fsstate_update(cd):
    t0 = time.time()
    cd.update()
    t1 = time.time()
    log_notice("updating fs state took %.3f seconds" % (t1-t0))

def fsstate_collect_directories(node, typeid, hits):
    # If this directory contains files of given type, it is a match
    for child in node.files():
        if child.filetype() == typeid:
            hits.append(node)
            break
    # Subdirectory recursion
    for child in node.dirs():
        fsstate_collect_directories(child, typeid, hits)

def fsstate_collect_object_directories(root):
    # Locate build directory
    objects = root.locate_dir(TARGET_BASE)
    if objects == None:
        log_crit("build directory not found")
        sys.exit(1)

    # Find directories that have object files within them
    dirs_with_objects = []
    fsstate_collect_directories(objects, FT_OBJECT, dirs_with_objects)
    if not dirs_with_objects:
        log_crit("no object directories found")
        sys.exit(1)

    return dirs_with_objects

def fsstate_locate_source_for_target_directory(root, target_directory):
    # TARGET_BASE/foo/bar -> foo/bar
    target_path = target_directory.path()
    assert target_path.startswith(TARGET_BASE)
    source_path = target_path.split("/", 1)[1]
    source_directory = root.locate_dir(source_path)
    return source_directory

# ============================================================================
# MAIN
# ============================================================================

BOOLEAN_STRINGS_TRUE  = ( "true",  "yes", "on"  )
BOOLEAN_STRINGS_FALSE = ( "false", "no",  "off" )

def parse_bool(text):
    if text in BOOLEAN_STRINGS_TRUE:
        return True
    if text in BOOLEAN_STRINGS_FALSE:
        return False
    return parse_int(text) > 0

def parse_int(text):
    try:
        return int(text)
    except:
        log_crit("%s is not a number" % text)
        sys.exit(1)

def component_sort_key(dirpath):
    # key generator for component sorting
    # deeper subdirs first, in alpha order
    return (-dirpath.count("/"), dirpath)

def probe_build_environment():
    global PROBED_WITHIN_SBOX_TARGET
    if "SBOX_SESSION_DIR" in  os.environ:
        PROBED_WITHIN_SBOX_TARGET = True
    elif os.path.isdir("/parentroot"):
        PROBED_WITHIN_SBOX_TARGET = False
    else:
        log_crit("This script needs to be run withing scratchbox chroot or target")
        sys.exit(1)

def execute_action_list(actions):
    # Prelude: echo commands, exit on failure, setup environment
    script_text = [
    "#!/bin/sh",
    "set -e -x",
    ". obj-build-mer-qt-xr/rpm-shared.env",
    ]
    script_text.extend(actions)
    script_text.append('')
    script_text = "\n".join(script_text)

    # determine how to execute the script
    cmd = os.path.realpath(ACTIONS_SCRIPT)
    if not PROBED_WITHIN_SBOX_TARGET:
        if OPTION_USE_SBOX_TARGET:
            cmd = "-t %s %s" % (OPTION_USE_SBOX_TARGET, cmd)
        cmd = "sb2 %s" % cmd
    cmd = "time %s" % cmd
    log_notice("run: %s" % cmd)

    # handle --dry-run
    if OPTION_DRY_RUN:
        sys.stdout.write(script_text)
        return

    # write script and execute it
    script_file = open(ACTIONS_SCRIPT, "w")
    script_file.write(script_text)
    script_file.close()
    os.chmod(ACTIONS_SCRIPT, 0o775)

    sys.stdout.flush()
    sys.stderr.flush()
    xc = os.system(cmd)
    if xc != 0:
        log_crit("actions failed: exit code = %d" % xc)
        # leave script in place
        sys.exit(1)

    # remove script on success
    os.unlink(ACTIONS_SCRIPT)

def output_usage():
    progname = os.path.basename(sys.argv[0])
    print("""
    Embedlite build helper

    Description:
        Full gecko-dev rpm builds can take hours to finish.

        If such build fails during development cycle, it can be continued
        manually as long as configuration steps were successfully completed.
        However tracking what parts of the build were left unfinished can
        be time consuming and complicated.

        This script automates some aspects of continuing failed builds
        and/or rebuilding after making modifications to source tree.

    Usage:
        %s [option] ... [component] ...

    Options:
        -h --help        This help text.
        -v --verbose     Make diagnostic logging more verbose.
        -q --quiet       Make diagnostic logging less verbose.
        --target=<name>  Specify scratchbox target. [use sbox defaults]
        --auto=<yes|no>  Whether to autodetect components to rebuild. [yes]
        --link=<yes|no>  Whether to rebuild libxul library. [no]
        --strip=<yes|no> Whether to strip libxul library. [no]
        --dry-run        Just output actions instead of executing them.

    Components:
        Either build or source directory path can be used, for example:
            "intl/locale/gtk", or
            "%s/intl/locale/gtk"

        Any number of components can be specified, for example two:
            intl/locale/gtk gfx/thebes

    Notes:
        Due to how python getopt works, if both options and components are
        given, all options must be specified before the 1st compontent.

        While the build script can be executed both from outside and within
        sbox target, the latter renders --target option useless and scanning
        is likely to be much slower (if python is running as qemu process).
    """.replace("\n    ", "\n") % (progname,  TARGET_BASE))
    sys.exit(0)

def main():
    global OPTION_USE_SBOX_TARGET
    global OPTION_DRY_RUN

    # Set of components to update
    components_to_update = {}

    # Automatically detect components to update
    use_heuristics = True

    # Link libxul.so after rebuilding components
    linking_wanted = False

    # Strip libxul.so after linking it
    stripping_wanted = False

    # Handle command line options
    opt_S = "hqv"
    opt_L = [ "help", "quiet", "verbose", "target=", "auto=", "link=", "strip=", "dry-run" ]

    try:
        opts, args = getopt.getopt(sys.argv[1:], opt_S, opt_L)
    except getopt.GetoptError as err:
        log_err(err) # Assumed to print something sane
        log_err("Use --help for more information")
        sys.exit(1)

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            output_usage()
        elif opt in ("-q", "--quiet"):
            log_less_verbose()
        elif opt in ("-v", "--verbose"):
            log_more_verbose()
        elif opt in ("--strip", ):
            stripping_wanted = parse_bool(arg)
        elif opt in ("--link", ):
            linking_wanted = parse_bool(arg)
        elif opt in ("--auto", ):
            use_heuristics  = parse_bool(arg)
        elif opt in ("--target"):
            OPTION_USE_SBOX_TARGET = arg
        elif opt in ("--dry-run"):
            OPTION_DRY_RUN = True
        else:
            log_err("Unhandled option: %s" % opt)
            sys.exit(1)

    # Handle command line arguments
    for arg in args:
        if not arg.startswith(TARGET_BASE):
            arg = os.path.join(TARGET_BASE, arg)
        if not os.path.isdir(arg):
            log_err("%s: is not a target directory" % arg)
            sys.exit(1)
        components_to_update[arg] = None

    # Verify that we are running either in sdk chroot or sbox2 target
    probe_build_environment()

    # Auto-detect components that need to be rebuild
    if use_heuristics:
        log_notice("scanning source tree ...")
        root = fsstate_load()
        fsstate_update(root)
        fsstate_save(root)

        log_notice("locating directories to rebuild")
        dirs_with_objects = fsstate_collect_object_directories(root)
        need_uptodate_header = False
        for object_dir in dirs_with_objects:
            source_dir = fsstate_locate_source_for_target_directory(root, object_dir)
            if source_dir == None:
                continue
            source_files = source_dir.source_files()
            if not source_files:
                continue

            object_files = object_dir.object_files()

            object_mtime = max(map(lambda x:x.timestamp(), object_files))
            source_mtime = max(map(lambda x:x.timestamp(), source_files))

            if object_mtime >= source_mtime:
                if need_uptodate_header:
                    need_uptodate_header = False
                    log_info("---- up to date ----")
                log_info("object_dir: %s" % object_dir.path())
                continue

            log_notice("---- rebuild needed ----")
            log_notice("object_dir: %s" % object_dir.path())
            log_info("   objects: %s" % " ".join(map(str, object_files)))
            log_info("source_dir: %s" % source_dir.path())
            log_info("   sources: %s" % " ".join(map(str, source_files)))
            components_to_update[object_dir.path()] = None
            need_uptodate_header = True

    # Queue actions to take
    actions = []

    for object_dir in sorted(components_to_update, key=component_sort_key):
        log_notice("schedule build in %s" % object_dir)
        actions.append("make -j16 -C %s" % object_dir)

    if linking_wanted:
        log_notice("schedule libxul linking")
        # By default make does targets:
        #   artifact pre-export export compile misc libs tools check
        # For our purposes doing just "compile" suffices
        actions.append("make -C obj-build-mer-qt-xr/toolkit/library compile")

    if stripping_wanted:
        log_notice("schedule libxul stripping")
        actions.append("strip obj-build-mer-qt-xr/toolkit/library/libxul.so")

    # Execute actions
    if not actions:
        log_notice("no actions to take")
    else:
        log_notice("executing scheduled actions")
        execute_action_list(actions)

    # Done
    log_notice("finished")

if __name__ == "__main__":
    if 0:
        import cProfile
        cProfile.run("main()")
    else:
        main()
