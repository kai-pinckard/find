#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
/*
The file type and mode
       The  stat.st_mode field (for statx(2), the statx.stx_mode field) contains the file type and
       mode.

       POSIX refers to the stat.st_mode bits corresponding to the mask S_IFMT (see below)  as  the
       file  type, the 12 bits corresponding to the mask 07777 as the file mode bits and the least
       significant 9 bits (0777) as the file permission bits.

       The following mask values are defined for the file type:

           S_IFMT     0170000   bit mask for the file type bit field

           S_IFSOCK   0140000   socket
           S_IFLNK    0120000   symbolic link
           S_IFREG    0100000   regular file
           S_IFBLK    0060000   block device
           S_IFDIR    0040000   directory
           S_IFCHR    0020000   character device
           S_IFIFO    0010000   FIFO
*/

/*
    Returns a new char* containing the directory and the file name
    joined into a valid file path. Caller is responsible for deallocating
    the returned char*.
*/



bool handle_name()
{

}

bool handle_mtime()
{

}

bool handle_type()
{

}

bool handle_exec()
{

}

bool handle_print()
{

}

bool handle_L()
{

}


char* join_path(const char* directory, const char* file_name)
{
    // one extra space in case we need to add a / later
    // Dangersous change later
    int total_length = strlen(directory) + strlen(file_name) + 2;
    char* path = (char*) calloc(total_length, sizeof(char));
    return strcat(strcat(path, directory), file_name);
}

/*
    Recursively traverses the directory applying the specified function on each file
*/
void walk_dir(char* directory)
{
    struct dirent *de;
    struct stat statbuffer;
    printf("%s\n", directory);
    DIR* dr = opendir(directory);

    if (dr == NULL)
    {
        perror("unable to open directory");
    }

    while((de = readdir(dr)) != NULL)
    {
        // Check that the current item is not . or ..
        if ( (strcmp(de->d_name, "..") != 0 ) && (strcmp(de->d_name, ".") != 0) )
        {
            char* path = join_path(directory, de->d_name);

            // check if de->d_type is available for performance
            // fall back on calling stat.
            if(stat(path, &statbuffer) == -1)
            {
                perror("Unable to get stat info for input file");
            }

            // Check if the file is of type directory
            if((statbuffer.st_mode & S_IFMT) == S_IFDIR)
            {
                //printf("%s\n", path);
                strcat(path, "/");
                walk_dir(path);
            }
            else
            {
                printf("%s\n", path);
            }
            free(path);
        }
    }
    closedir(dr);
}


int main(int argc, char** argv)
{
    walk_dir(argv[1]);
    return 0;
}