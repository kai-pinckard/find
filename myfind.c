#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fnmatch.h>
#include <time.h>
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


// Globals consider removing
char* pattern;
int num_days;
bool debug = true;

// start using struct to store information.
typedef struct 
{
    // for example ../testdir/file.txt
    // or ./subdir/subsubdir/
    const char* path;
    // for example file.txt
    // subsubdir
    const char* file_name;
    // stores the information from calling stat
    struct stat statbuffer;

} file_data_t;

/*
Returns a new char* containing the directory with the last / removed
For example ../exampledir/ becomes ../exampledir. Caller is responsible for 
deallocating returned char*.
*/
char* remove_last_slash(const char* directory)
{
    return strndup(directory, strlen(directory) - 1);
}

/*

    For example ../dir/exampledir/ becomes exampledir

*/
char* dir_path_to_dir_name(const char* directory)
{
    char* dir_path_no_slash = remove_last_slash(directory);

    // get pointer to last occurance of char
    char* start_pos = strrchr(dir_path_no_slash, '/');
    //printf("debug %s\n", start_pos);
    // copy str starting from the next char on.
    char* dir_name = strdup(start_pos + 1);
    free(dir_path_no_slash);
    return dir_name;
}

/*
    Takes a complete path as an input such as ../testdir/dir/file.txt or ../testdir/otherdir/
    prints out the path minus the last slash if it is present
*/
void print_match(const char* path)
{
    int length = strlen(path);

    if(path[length - 1] == '/')
    {
        length--;
    }

    for (int i = 0; i < length; i++)
    {
        if(putchar(path[i]) == EOF)
        {
            perror("Can't write to stdout");
        }  
    }
    if(putchar('\n') == EOF)
    {
        perror("Can't write to stdout");
    }  
}

/*
    Returns true if the file_name matches the pattern. The pattern
    is simply whatever was passed for -name
*/
bool handle_name(const char* pattern, const char* file_name)
{
    //printf("handling name with pattern %s\n", pattern);
    return fnmatch(pattern, file_name, 0) == 0;
}



/*
    Returns true if the modified time is within num_days of the
    current time.
*/
bool occurred_within(const time_t current_time, const time_t m_time, const int num_days)
{
    const long int num_secs_per_day = 86400;
    long int max_dif = num_secs_per_day * num_days;
    return (current_time - m_time) < max_dif;
}
/*
    Returns true if the file specified by file_name was modified at
    mtime. note +/- features not implemented. 
*/
bool handle_mtime(const file_data_t file)
{
    //printf("handling mtime\n");
    //struct tm* time = localtime(&(file.statbuffer.st_mtime));
    //if (time != NULL)
    //{
    printf("it is %s \n", ctime(&(file.statbuffer.st_mtime)));
    //printf("%s\n", file.path);
    //printf("%ld\n\n", file.statbuffer.st_mtime);
    
    time_t cur_time;
    time(&cur_time);
    //current_time = localtime(&rawtime);
    return occurred_within(cur_time, file.statbuffer.st_mtime, num_days);
    //}
    //else
    //{
    //perror("unable to convert time.");
    //}
    //free(time);
}

/*
    Returns true if the file specified by file_name is of the type
    passed for -type
*/
bool handle_type()
{
    //printf("handling type\n");
    return true;
}

/*
    executes the specified command on all the file specified.
*/
bool handle_exec()
{
    //printf("handling exec\n");
    return true;
}

/*
    prints the specified file.
*/
bool handle_print()
{
    //printf("handling print\n");
    return true;
}

bool handle_L()
{
    //printf("handling L\n");
    return true;
}

/*
    returns true if a file meets the requirements.
    returns false otherwise.
*/
void handle_file(const file_data_t file)
{
    //if(is_dir)
    //{
    if(handle_name(pattern, file.file_name))
    {
        print_match(file.path);
        if(handle_mtime(file))
        {
            printf("%s recent enough\n", file.path);
        }
    }
    //handle_mtime(file);
    //}
/*     else
    {
        //printf("handling file %s\n", file_name);
        if(!handle_name(pattern, file_name))
        {
        }
        /*
        if(!handle_type(pattern, file_name))
        {
            return false;
        }
        if(!handle_mtime(pattern, file_name))
        {
            return false;
        }
        else
        {
            printf("%s\n", file_name);
        }
    } */
}

/*
    Returns a new char* containing the directory and the file name
    joined into a valid file path. Caller is responsible for deallocating
    the returned char*.
*/
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
void walk_dir(file_data_t dir_file_data)
{
    struct dirent *de;
    
    // Every directory handles it self at the beginning
    handle_file(dir_file_data);

    DIR* dr = opendir(dir_file_data.path);

    if (dr == NULL)
    {
        perror("unable to open directory");
    }

    while((de = readdir(dr)) != NULL)
    {
        // Check that the current item is not . or ..
        if ( (strcmp(de->d_name, "..") != 0 ) && (strcmp(de->d_name, ".") != 0) )
        {
            char* file_name = strdup(de->d_name);
            char* path = join_path(dir_file_data.path, de->d_name);
            struct stat statbuffer;
            file_data_t cur_file = {path, file_name, statbuffer};

            // check if de->d_type is available for performance
            // fall back on calling stat.
            if(stat(path, &cur_file.statbuffer) == -1)
            {
                perror("Unable to get stat info for input file");
            }

            // Check if the file is of type directory
            if((statbuffer.st_mode & S_IFMT) == S_IFDIR)
            {
                //printf("%s\n", path);
                strcat(path, "/");
                walk_dir(cur_file);
            }
            else
            {
                // None directories are handled here
                handle_file(cur_file);
                //printf("walking %s\n", path);
            }
            free(file_name);
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
    printf("------------------start parse--------------------\n");
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
                pattern = get_opt_args(argv, i);
                /* if (handle_name())
                {
                    printf("%s matched pattern: \n")
                } */
            }
            else if(strcmp(argv[i], "-mtime") == 0)
            {
                //handle_mtime();
                num_days = atoi(argv[i+1]);
                printf("num days: %d\n", num_days);
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
            printf("parsing %s\n",argv[i]);
        }
    }
    printf("----------------end parse--------------------\n");
}


int main(int argc, char** argv)
{

    parse_args(argc, argv);
    struct stat statbuffer;
    file_data_t base_dir = 
        {
            argv[1],
            dir_path_to_dir_name(argv[1]),
            statbuffer
        };
    if(stat(base_dir.path, &base_dir.statbuffer) == -1)
    {
        perror("Unable to get stat info for input file");
    }
    walk_dir(base_dir);
    return 0;
}