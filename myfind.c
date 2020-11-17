#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fnmatch.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <assert.h>
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
const int NUM_SECS_PER_DAY = 86400;
char* pattern = NULL;
int num_days = -1;
// this is supposed to match any file
mode_t desired_mode = S_IFMT;
char** exec_args = NULL;
int exec_argc = -1;
bool debug = true;
// maybe change
char* path = "/usr/bin /usr .";

// start using struct to store information.
typedef struct 
{
    // for example ../testdir/file.txt
    // or ./subdir/subsubdir/
    char* path;
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
    int last_char_index = strlen(directory) - 1;
    if(directory[last_char_index] == '/')
    {
        return strndup(directory, last_char_index);
    }
    return strdup(directory);
}

/*

    For example ../dir/exampledir/ becomes exampledir

*/
char* dir_path_to_dir_name(const char* directory)
{
    char* dir_path_no_slash = remove_last_slash(directory);

    // get pointer to last occurance of char
    char* start_pos = strrchr(dir_path_no_slash, '/');
    if(start_pos != NULL)
    {
        //printf("debug %s\n", start_pos);
        // copy str starting from the next char on.
        char* dir_name = strdup(start_pos + 1);
        free(dir_path_no_slash);
        return dir_name;
    }
    return dir_path_no_slash;
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
from wish
*/
void set_args_to_null(char** args, int start_index)
{
    for(int i = start_index; args[i] != NULL; i++)
    {
        args[i] = NULL;
    }
    return;
}

/*
From wish
change this to handle_Exec
*/
bool handle_external_command(file_data_t file)
{
    // Create a modifiable copy of path
    int path_len = strlen(path) + 1;
    char path_cp[path_len];
    strncpy(path_cp, path, path_len);

    char* directory = strtok(path_cp, " ");
    bool found_file = false;

    while(directory != NULL)
    {
        // Combine the directory with args[0]
        printf("directory: %s exec_Arg[0] %s\n", directory, exec_args[0]);
        int file_path_length = strlen(directory) + strlen(exec_args[0]) + 2;
        char* file_path = (char*) calloc(file_path_length, sizeof(char));
        strncpy(file_path, directory, file_path_length);
        strcat(file_path, "/");
        strcat(file_path, exec_args[0]);

        if (access(file_path, X_OK) == 0)
        {
            printf("accessed %s\n", file_path);
            // A valid executable has been found
            found_file = true;
            pid_t child;
            child = fork();

            if(child != 0)
            {
                // The parent returns childs return code
                int wstatus = -1;
                printf("waiting for child\n");
                waitpid(child, &wstatus, 0);
                printf("child returned with %d\n", wstatus);
                return !(wstatus == 0);
                
            }
            }
            else
            {
                int redirection_file = -1;

                // Loop through all arguments to see if any are ">"
                for(int i = 0; exec_args[i] != NULL; i++)
                {
                    if(strcmp(exec_args[i], ">") == 0)
                    {
                        // Check that there is an argument after '>' and there is not more than one
                        if(exec_args[i+1] != NULL && exec_args[i+2] == NULL)
                        {
                            redirection_file = open(exec_args[i+1], O_CREAT|O_TRUNC|O_WRONLY, 0666);
                            if (redirection_file < 0)
                            {
                                // Failed to open redirection file
                                perror("1 an error occured in handle_external");
                                return 1;
                            }
                            // Remove all args after and including > in the current command
                            set_args_to_null(exec_args, i);
                        }
                        else
                        {
                            perror("2 an error occured in handle_external");
                            return 1;
                        }
                        // No need to continue loop after finding '>'
                        break;
                    }
                }

                // Configure output and error redirection

                free(exec_args[0]);
                exec_args[0] = file_path;
                printf("file path: %s", exec_args[0]);

                dup2(redirection_file, 1);
                dup2(redirection_file, 2);

                printf("starting child with args:\n");
                for(int i =0; i < exec_argc; i++)
                {
                    printf("arg %d %s\n", i, exec_args[i]);
                }
                // Run the specified executable
                execv(exec_args[0], exec_args);
                perror("child returned with error");
                exit(1);
            }
            directory = strtok(NULL, " ");
        }
        if(!found_file)
        {
            perror("4 an error occured in handle_external");
        }
        return 1;
    }
    

/*
    Returns true if the modified time is within num_days of the
    current time.
*/
bool occurred_within(const time_t current_time, const time_t m_time, const int num_days)
{
    
    long int max_dif = NUM_SECS_PER_DAY * (num_days + 1);
    long int min_dif = num_days * num_days;
    time_t time_dif = current_time - m_time;
    return time_dif < max_dif && time_dif > min_dif;
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
    ////printf("it is %s \n", ctime(&(file.statbuffer.st_mtime)));
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
bool handle_type(const file_data_t file)
{
    //printf("handling type\n");
    return (file.statbuffer.st_mode & S_IFMT) == desired_mode;
}

/*
    executes the specified command on all the file specified.
    note that exec is also a filter with only files that return 0 passing
    note also that by default files are not printed that have exec called on them.
    however if print is also specified then all files that returned 0 from exec will also be printed.
    additionally exec needs to expand {} with the current file in all of its args and all args are assumed
    to be exec args until a arg ; is encountered. 
*/
bool handle_exec(file_data_t file)
{
    /* //printf("handling exec\n");
    int length = strlen(path) + strlen(exec_args[0]) + 2;
    char* exec_path = (char*) calloc(length, sizeof(char));
    strcat(strcat(exec_path, path), exec_args[0]);
    printf("printing execpath %s\n", exec_path);
    if (access(exec_path, X_OK) == 0)
    {
        // A valid executable has been found
        pid_t child;
        child = fork();

        if(child != 0)
        {
            // The parent waits for child
            int wstatus = -1;
            while(waitpid(child, &wstatus, 0) > 0);
        }
        else
        {
            execv(file.path, exec_args);
        }
    }
    else
    {
        perror("invalid executable.");
    } */
    return system(exec_args) != 0;
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
    //printf("%s\n", file.file_name);
    if(pattern != NULL && !handle_name(pattern, file.file_name))
    {
        return;
    }
    if(num_days != -1 && !handle_mtime(file))
    {
        return;
        //printf("%s recent enough\n", file.path);
    }
    if(desired_mode != S_IFMT && !handle_type(file))
    {
        return;
    }
    if(exec_args != NULL && !handle_exec(file))
    {
        return;
    }
    print_match(file.path);
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
            if((cur_file.statbuffer.st_mode & S_IFMT) == S_IFDIR)
            {
                //printf("%s\n", path);
                strcat(cur_file.path, "/");
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
    Takes a single mode char and returns the corresponding
    mode bit mask.
*/
mode_t get_mode_mask(char mode_specifier)
{
    switch(mode_specifier)
    {
        case 'b': return S_IFBLK;
        case 'c': return S_IFCHR;
        case 'd': return S_IFDIR;
        //case 'p': return 
        case 'f': return S_IFREG;
        case 'l': return S_IFLNK;
        case 's': return S_IFSOCK;
        //case 'D': return
        default: perror("unexpected mode");
        return S_IFMT;
    }
}


mode_t arg_to_mode(char** argv, int index)
{
    char* mode_specifier = argv[index+1];
    if(strlen(mode_specifier) != 1)
    {
        perror("invalid mode");
    }
    char mode = mode_specifier[0];
    mode_t mode_mask = get_mode_mask(mode);
}

/*
    Searches for the terminating ; and replaces it with NULL
    then returns the pointer to the first exec arg.

//////////////////////////////////////////
    //need to expand {} this will require not storing things in argv
*/
void get_exec_args(char** argv, int argc, int index)
{
    int semicolon_index = -1; 
    for(int i = index + 1; i < argc; i++)
    {
        printf("index %d argc %d\n", i, argc);
        printf("%s\n", argv[i]);
        // Check for the terminating ; 
        if (argv[i][0] == ';' && argv[i][1] == '\0')
        {
            semicolon_index = i; 
            break;
        }
    }
    assert(semicolon_index > index);
    exec_args = (char**) calloc( semicolon_index - index, sizeof(char*));
    for(int i = index + 1; i < semicolon_index; i++)
    {
        printf("debug %d %s\n", i, argv[i]);
        exec_args[i-index-1] = strdup(argv[i]);
        //printf("%s", exec_args[i-index+1]);
    }
    exec_argc = semicolon_index - index -1;
    return;
    //fix this
    perror("find: missing argument to `-exec'");
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
                //handle_type();
                desired_mode = arg_to_mode(argv, i);
                printf("desired mode: %d\n", desired_mode);
            }
            else if(strcmp(argv[i], "-exec") == 0)
            {
                // name of executable is a required argument.
                printf("parsing exec\n");
                get_exec_args(argv, argc, i);
                int j = 0;
                for(int j = 0; j < exec_argc; j++)
                {
                    printf("%s ", exec_args[j]);
                }
                printf("\n");
                continue;
                
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

    /* use realpath
    througout for path resolution and link handling.
    */
    //parse_args(argc, argv);
    struct stat statbuffer;
    char* base_path;

    if(!starts_with(argv[1], "-"))
    {
        base_path = argv[1];
    }
    else
    {
        base_path = ".";
    }

    file_data_t base_dir = 
        {
            base_path,
            dir_path_to_dir_name(argv[1]),
            statbuffer
        };
    if(stat(base_dir.path, &base_dir.statbuffer) == -1)
    {
        perror("Unable to get stat info for input file");
    }
    //handle_external_command(base_dir);
    system("grep percent findman.txt > output.txt");
    //walk_dir(base_dir);
    return 0;
}