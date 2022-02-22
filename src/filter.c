#include <fnmatch.h>
#include <string.h>
#include <stdlib.h>

#include "filter.h"

char *ffstatus_str_arr[] = {
    [1+filefilter_status_found] = "Matching pattern found",
    [1+filefilter_status_ok] = "Success",
    [1+filefilter_status_notfound] = "Matching pattern not found",
    [1+filefilter_status_incorrect_name] = "Incorrect matching pattern",
    [1+filefilter_status_incorrect_mode] = "Incorrect file type",
    [1+filefilter_status_addfail] = "Inserting pattern failed",
    [1+filefilter_status_dupfound] = "Duplicate found"
};

struct FileFilter {
    char **name;
    FFType *type;
    char *has_wildcard;
};

FileFilter *filefilter_create()
{
    FileFilter* f = (FileFilter*)malloc(sizeof(FileFilter));
    f->name = (char**)malloc(sizeof(char*));
    f->type = (FFType*)malloc(sizeof(FFType));
    f->has_wildcard = (char*)malloc(sizeof(char));
    f->name[0] = NULL;
    f->type[0] = 0;
    f->has_wildcard[0] = 0;
    return f;
}

void filefilter_destroy(FileFilter *f)
{
    int i = 0;

    while(f->name[i++] != NULL)
        free(f->name[i]);
    free(f->name);
    free(f->type);
    free(f->has_wildcard);
    free(f);
}

FFStatus filefilter_add(FileFilter *f, char *spec, FFType type)
{
    int pos = -1;
    char *newname = NULL;

    if (strlen(spec) == 0)
        return filefilter_status_incorrect_name;

    if (!(type&FFT_ANY))
        return filefilter_status_incorrect_mode;

    if (strchr(spec,'/'))
        return filefilter_status_incorrect_name;

    newname = strdup(spec);

    while(f->name[++pos] != NULL) {
        if (strcmp(f->name[pos],newname) == 0)
            return filefilter_status_dupfound;
    };
    f->name = (char**)realloc(f->name, sizeof(char*)*(pos+2));
    f->type = (FFType*)realloc(f->type, sizeof(FFType)*(pos+2));
    f->has_wildcard = (char*)realloc(f->has_wildcard, sizeof(char)*(pos+2));

    f->name[pos+1] = NULL;
    f->type[pos+1] = 0;
    f->has_wildcard[pos+1] = 0;

    f->name[pos] = newname;
    f->type[pos] = type;

    /* 
     * Here we try to opportunistically determine whether we have
     * wildcard patterns specified in glob(7), to improve performance
     * of simple specs contains exact names.
     *
     * Due to complexity of ranges syntax enclosed in square brackets,
     * we don't fully check the spec for its presence, just consider this
     * is a glob pattern if one of globbing (escaped or not) characters is found.
     *
     * This just leaves false-positives unoptimized, but fnmatch() should
     * handle them correctly.
     *
     * TODO?
     * */
    if (strpbrk(newname,"*?[]")) {
        f->has_wildcard[pos] = 1;
    } else {
        f->has_wildcard[pos] = 0;
    };

    return filefilter_status_ok;
}

FFStatus filefilter_find_match(FileFilter *f, char *fn, mode_t type)
{
    int pos = -1;
    FFType type_b = MODET_TO_BITMASK(type);

    if (strlen(fn) == 0) {
        return filefilter_status_incorrect_name;
    };
    if (!(type_b&FFT_ANY)) {
        return filefilter_status_incorrect_mode;
    };

    while(f->name[++pos] != NULL) {
        if (!(f->type[pos]&type_b))
            continue;
        if (f->has_wildcard[pos]) {
            if (fnmatch(f->name[pos],fn,0) == 0)
                return filefilter_status_found;
        } else {
            if (strcmp(f->name[pos],fn) == 0)
                return filefilter_status_found;
        };
    };

    return filefilter_status_notfound;
}
