#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fnmatch.h>

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


/*
    Returns true if the file_name matches the pattern. The pattern
    is simply whatever was passed for -name
*/
bool handle_name(const char* pattern, const char* file_name)
{
    printf("handling name\n");
    return fnmatch(pattern, file_name, 0) == 0;
}

/*
    Returns true if the file specified by file_name was modified at
    mtime. note +/- features not implemented. 
*/
bool handle_mtime()
{
    printf("handling mtime\n");
    return true;
}

/*
    Returns true if the file specified by file_name is of the type
    passed for -type
*/
bool handle_type()
{
    printf("handling type\n");
    return true;
}

/*
    executes the specified command on all the file specified.
*/
bool handle_exec()
{
    printf("handling exec\n");
    return true;
}

/*
    prints the specified file.
*/
bool handle_print()
{
    printf("handling print\n");
    return true;
}

bool handle_L()
{
    printf("handling L\n");
    return true;
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


/*
    Returns true if the str starts with the pattern
    false otherwise.
*/
bool starts_with(const char* str, const char* pattern)
{
    for(int i = 0; i < strlen(pattern); i++)
    {
        if(str[i] != pattern[i])
        {
            return false;
        }
    }
    return true;
}

/*
    Gets the arguments for a particular option.
    Index the index into argv of the current option.
    may need to be updated if there are multiple args for an option.
*/
char* get_opt_args(char** argv, int index)
{
    return argv[index + 1];
}

/*
    Returns the next argument fol
*/
char* parse_args(int argc, char** argv)
{
    bool more_start_dirs = true;
    for(int i = 1; i < argc; i++)
    {
        printf("%s\n",argv[i]);

        // Check if it is an option flag
        if(starts_with(argv[i], "-"))
        {
            more_start_dirs = false;
            printf("option %s\n", argv[i]);

            if(strcmp(argv[i], "-name") == 0)
            {
                handle_name(get_opt_args(argv, i), "test");
            }
            else if(strcmp(argv[i], "-mtime") == 0)
            {
                handle_mtime();
            }
            else if(strcmp(argv[i], "-type") == 0)
            {
                handle_type();
            }
            else if(strcmp(argv[i], "-exec") == 0)
            {
                handle_exec();
            }
            else if(strcmp(argv[i], "-print") == 0)
            {
                handle_print();   
            }
            else if(strcmp(argv[i], "-L") == 0)
            {
                handle_L();
            }
        }
        else
        {
            printf("%s\n",argv[i]);
        }
    }
}


int main(int argc, char** argv)
{

    parse_args(argc, argv);
    //walk_dir(argv[1]);
    return 0;
}