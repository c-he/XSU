/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * VFS operations relating to pathname translation
 */
#include <assert.h>
#include <kern/errno.h>
#include <xsu/fs/fs.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/utils.h>
#ifdef VFS_DEBUG
#include <driver/vga.h>
#endif

static struct vnode* bootfs_vnode = NULL;

/*
 * Helper function for actually changing bootfs_vnode.
 */
static void change_bootfs(struct vnode* newvn)
{
    struct vnode* oldvn;

    oldvn = bootfs_vnode;
    bootfs_vnode = newvn;

    if (oldvn != NULL) {
        VOP_DECREF(oldvn);
    }
}

/*
 * Common code to pull the device name, if any, off the front of a
 * path and choose the vnode to begin the name lookup relative to.
 */
static int getdevice(char* path, char** subpath, struct vnode** startvn)
{
    int slash = -1, colon = -1, i;
    struct vnode* vn;
    int result;

    /*
	 * Locate the first colon or slash.
	 */

    for (i = 0; path[i]; i++) {
        if (path[i] == ':') {
            colon = i;
            break;
        }
        if (path[i] == '/') {
            slash = i;
            break;
        }
    }

    if (colon < 0 && slash != 0) {
        /*
		 * No colon before a slash, so no device name
		 * specified, and the slash isn't leading or is also
		 * absent, so this is a relative path or just a bare
		 * filename. Start from the current directory, and
		 * use the whole thing as the subpath.
		 */
        *subpath = path;
        return vfs_getcurdir(startvn);
    }

    if (colon > 0) {
        // device:path - get root of device's filesystem.
        path[colon] = 0;
        while (path[colon + 1] == '/') {
            // device:/path - skip slash, treat as device:path.
            colon++;
        }
        *subpath = &path[colon + 1];
#ifdef VFS_DEBUG
        kernel_printf("path: %s\n", path);
        if (**subpath) {
            kernel_printf("subpath: %s\n", *subpath);
        } else {
            kernel_printf("There is no subpath.\n");
        }
#endif
        result = vfs_getroot(path, startvn);
        if (result) {
            return result;
        }

        return 0;
    }

    /*
	 * We have either /path or :path.
	 *
	 * /path is a path relative to the root of the "boot filesystem".
	 * :path is a path relative to the root of the current filesystem.
	 */
    assert(colon == 0 || slash == 0, "path format is incorrect.");

    if (path[0] == '/') {
        if (bootfs_vnode == NULL) {
            return ENOENT;
        }
        VOP_INCREF(bootfs_vnode);
        *startvn = bootfs_vnode;
    } else {
        assert(path[0] == ':', "path format is incorrect.");

        result = vfs_getcurdir(&vn);
        if (result) {
            return result;
        }

        /*
		 * The current directory may not be a device, so it
		 * must have a fs.
		 */
        assert(vn->vn_fs != NULL, "current directory does not have a fs.");

        *startvn = FSOP_GETROOT(vn->vn_fs);

        VOP_DECREF(vn);
    }

    while (path[1] == '/') {
        /* ///... or :/... */
        path++;
    }

    *subpath = path + 1;

    return 0;
}

/*
 * Set bootfs_vnode.
 *
 * Bootfs_vnode is the vnode used for beginning path translation of
 * pathnames starting with /.
 *
 * It is also incidentally the system's first current directory.
 */
int vfs_setbootfs(const char* fsname)
{
    char* tmp;
    char* s;
    int result;
    struct vnode* newguy;

    tmp = kernel_strdup(fsname);
    s = kernel_strchr(tmp, ':');
    if (s) {
        // If there's a colon, it must be at the end.
        if (kernel_strlen(s) > 0) {
            return EINVAL;
        }
    } else {
        kernel_strcat(tmp, ":");
    }
#ifdef VFS_DEBUG
    kernel_printf("fsname: %s\n", tmp);
#endif

    result = vfs_chdir(tmp);
    if (result) {
        return result;
    }

    result = vfs_getcurdir(&newguy);
    if (result) {
        return result;
    }

    change_bootfs(newguy);

    return 0;
}

int vfs_lookup(char* path, struct vnode** retval)
{
    struct vnode* startvn;
    int result;

    result = getdevice(path, &path, &startvn);
    if (result) {
        return result;
    }

    if (kernel_strlen(path) == 0) {
        *retval = startvn;
        return 0;
    }

    result = VOP_LOOKUP(startvn, path, retval);

    VOP_DECREF(startvn);
    return result;
}