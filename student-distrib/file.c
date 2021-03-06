#include "file.h"
#include "lib.h"
#include "debug.h"
#include "rtc.h"
#include "filesys.h"
#include "terminal.h"
#include "process.h"

/* Terminal stdin file ops */
static file_ops_t fops_stdin = {
    .open = terminal_kbd_open,
    .read = terminal_stdin_read,
    .write = terminal_stdin_write,
    .close = terminal_kbd_close
};

/* Terminal stdout file ops */
static file_ops_t fops_stdout = {
    .open = terminal_kbd_open,
    .read = terminal_stdout_read,
    .write = terminal_stdout_write,
    .close = terminal_kbd_close
};

/* File (the real kind) file ops */
static file_ops_t fops_file = {
    .open = fs_open,
    .read = fs_file_read,
    .write = fs_write,
    .close = fs_close
};

/* Directory file ops */
static file_ops_t fops_dir = {
    .open = fs_open,
    .read = fs_dir_read,
    .write = fs_write,
    .close = fs_close
};

/* RTC file ops */
static file_ops_t fops_rtc = {
    .open = rtc_open,
    .read = rtc_read,
    .write = rtc_write,
    .close = rtc_close
};

/* Mouse file ops */
static file_ops_t fops_mouse = {
    .open = terminal_mouse_open,
    .read = terminal_mouse_read,
    .write = terminal_mouse_write,
    .close = terminal_mouse_close
};

/* Initializes the file object from the given dentry */
static bool
init_file_obj(file_obj_t *file, dentry_t *dentry)
{
    file->inode_idx = 0;
    file->offset = 0;
    switch (dentry->type) {
    case FTYPE_RTC:
        file->ops_table = &fops_rtc;
        break;
    case FTYPE_DIR:
        file->ops_table = &fops_dir;
        break;
    case FTYPE_FILE:
        file->ops_table = &fops_file;
        file->inode_idx = dentry->inode_idx;
        break;
    case FTYPE_MOUSE:
        file->ops_table = &fops_mouse;
        break;
    default:
        debugf("Unknown file type: %d\n", dentry->type);
        return false;
    }

    file->valid = true;
    return true;
}

/* Gets the file object array for the executing process */
static file_obj_t *
get_executing_file_objs(void)
{
    pcb_t *pcb = get_executing_pcb();
    ASSERT(pcb != NULL);
    return pcb->files;
}

/* Gets the file object corresponding to the given descriptor */
static file_obj_t *
get_executing_file_obj(int32_t fd)
{
    file_obj_t *file;

    /* Ensure descriptor is in bounds */
    if (fd < 0 || fd >= MAX_FILES) {
        return NULL;
    }

    /* Get file object, check that it's open */
    file = &get_executing_file_objs()[fd];
    if (!file->valid) {
        return NULL;
    }

    return file;
}

/*
 * Initializes the specified file object array.
 */
void
file_init(file_obj_t *files)
{
    files[FD_STDIN].valid = true;
    files[FD_STDIN].ops_table = &fops_stdin;
    files[FD_STDOUT].valid = true;
    files[FD_STDOUT].ops_table = &fops_stdout;
    int32_t i;
    for (i = 2; i < MAX_FILES; ++i) {
        files[i].valid = false;
    }
}

/* open() syscall handler */
__cdecl int32_t
file_open(const uint8_t *filename)
{
    /* Ensure the string is valid */
    if (!is_user_readable_string(filename)) {
        return -1;
    }

    dentry_t dentry;
    int32_t i;
    file_obj_t *files = get_executing_file_objs();

    /* Skip fd = 0 (stdin) and fd = 1 (stdout) */
    for (i = 2; i < MAX_FILES; ++i) {
        if (!files[i].valid) {
            /* Try to read filesystem entry */
            if (read_dentry_by_name(filename, &dentry) != 0) {
                return -1;
            }

            /* Initialize file object */
            if (!init_file_obj(&files[i], &dentry)) {
                return -1;
            }

            /* Perform post-initialization setup */
            if (files[i].ops_table->open(filename, &files[i]) != 0) {
                files[i].valid = false;
                return -1;
            }

            /* Index becomes our file descriptor */
            return i;
        }
    }

    /* Too many files open */
    return -1;
}

/* read() syscall handler */
__cdecl int32_t
file_read(int32_t fd, void *buf, int32_t nbytes)
{
    file_obj_t *file = get_executing_file_obj(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->read(file, buf, nbytes);
}

/* write() syscall handler */
__cdecl int32_t
file_write(int32_t fd, const void *buf, int32_t nbytes)
{
    file_obj_t *file = get_executing_file_obj(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->write(file, buf, nbytes);
}

/* close() syscall handler */
__cdecl int32_t
file_close(int32_t fd)
{
    file_obj_t *file = get_executing_file_obj(fd);
    if (file == NULL) {
        return -1;
    }
    if (file->ops_table->close(file) != 0) {
        return -1;
    }
    file->valid = false;
    return 0;
}
