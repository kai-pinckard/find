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

// Globals consider removing
const int NUM_SECS_PER_DAY = 86400;
char* pattern = NULL;
int num_days = -1;
// this is supposed to match any file
mode_t desired_mode = S_IFMT;
char* exec_args = NULL;
bool debug = true;
bool should_print = false;
bool follow_symbolic = false;

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

int num_base_dirs = 0;
file_data_t* base_dirs = NULL;
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
    time_t cur_time;
    time(&cur_time);
    return occurred_within(cur_time, file.statbuffer.st_mtime, num_days);
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
    fill the brackets in exec_args and return a new string caller must free
*/
char* populate_command(file_data_t file)
{
    // calculate allocation size of filled string
    int path_length = strlen(file.path);
    int length = strlen(exec_args) + 1;
    int size_inc = path_length - strlen("{}");

    // loop through all but the last char in the string
    // to find occurances of {}
    for(int i = 0; exec_args[i+1] != '\0'; i++)
    {
        if (exec_args[i] == '{' && exec_args[i+1] == '}')
        {
            length += size_inc;
        }
    }

    char* filled_exec_args = (char*) calloc(length, sizeof(char));

    int filled_str_index = 0;
    for (unsigned long int original_index = 0; original_index < strlen(exec_args);)
    {
        if (exec_args[original_index] == '{' && exec_args[original_index+1] == '}')
        {
            for(int j = 0; j < path_length; j++)
            {
                filled_exec_args[filled_str_index + j] = file.path[j];
            }
            filled_str_index += path_length;
            original_index += 2;
        }
        else
        {
            filled_exec_args[filled_str_index] = exec_args[original_index];
            filled_str_index += 1;
            original_index += 1;
        }
    }
    //printf("demo fill %s\n", filled_exec_args);
    return filled_exec_args;
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
    char* filled_exec_args = populate_command(file);
    return system(filled_exec_args) == 0;
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
    if(exec_args == NULL || should_print)
    {
        print_match(file.path);
    }
}


/*
    returns a new string that the caller must deallocate that contains the trailing
    slash.
*/
char* add_slash(const char* directory)
{
    int length = strlen(directory) + 2;
    char* dir = (char*) calloc(length, sizeof(char));
    return strcat(strcat(dir, directory), "/");
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
    int dir_len = strlen(directory);
    int total_length = dir_len + strlen(file_name) + 1;
    bool needs_slash = directory[dir_len-1] != '/';

    if (needs_slash)
    {
        total_length++;
    }

    char* path = (char*) calloc(total_length, sizeof(char));
    strcat(path, directory);

    if(needs_slash)
    {
        strcat(path, "/");
    }

    return strcat(path, file_name);
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

            /* if (follow_symbolic)
            {
                //printf("not resolved: %s\n", path);
                char* resolved_path = realpath(path, NULL);
                // need to fix missing / in inputs.
                free(path);
                path = resolved_path;
                //printf("resolved: %s\n", path);
            } */

            struct stat statbuffer;
            if(follow_symbolic)
            {
                if(stat(path, &statbuffer) == -1)
                {
                    perror("Unable to get stat info for input file");
                }
            }
            else
            {
                if(lstat(path, &statbuffer) == -1)
                {
                    perror("Unable to get stat info for input file");
                }
            }
            file_data_t cur_file = {path, file_name, statbuffer};

            // check if de->d_type is available for performance
            // fall back on calling stat.
            //printf("%s\n", path);

            // Check if the file is of type directory
            if((cur_file.statbuffer.st_mode & S_IFMT) == S_IFDIR)
            {
                if((cur_file.statbuffer.st_mode & S_IFMT) != S_IFLNK || follow_symbolic)
                {
                    //printf("%s\n", path);
                    char* new_path = add_slash(cur_file.path);
                    free(cur_file.path);
                    cur_file.path = new_path;
                    walk_dir(cur_file);
                }

            }
            else
            {
                // None directories are handled here
                handle_file(cur_file);
                //printf("walking %s\n", path);
            }
            free(file_name);
            //free(path);
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
    for(unsigned long int i = 0; i < strlen(pattern); i++)
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
    return mode_mask;
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

    // start at one so we dont have to add space for terminating char later.
    int exec_args_len = 1;
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
        else
        {
            exec_args_len += strlen(argv[i]) + 1;
        }
    }
    assert(semicolon_index > index);
    exec_args = (char*) calloc(exec_args_len, sizeof(char));
    for(int i = index + 1; i < semicolon_index; i++)
    {
        printf("debug %d %s\n", i, argv[i]);
        strcat(strcat(exec_args, argv[i]), " ");
    }

    return;
    //fix this
    perror("find: missing argument to `-exec'");
}
/*
    Returns the next argument fol
*/
void parse_args(int argc, char** argv)
{
    printf("------------------start parse--------------------\n");
    bool more_start_dirs = true;
    for(int i = 1; i < argc; i++)
    {
        printf("%s\n",argv[i]);

        if(strcmp(argv[i], "-L") == 0)
        {
            follow_symbolic = true;
        }
        // Check if it is an option flag
        else if(starts_with(argv[i], "-"))
        {
            more_start_dirs = false;
            printf("option %s\n", argv[i]);

            if(strcmp(argv[i], "-name") == 0)
            {
                pattern = get_opt_args(argv, i);
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
                printf("%s ", exec_args);
                printf("\n");
                continue;
                
            }
            else if(strcmp(argv[i], "-print") == 0)
            {
                should_print = true;  
            }
        }
        else if(more_start_dirs)
        {
            struct stat statbuffer;
            char* base_path;
            int base_dirs_index = num_base_dirs;

            // need to fix problem with -L coming before the files.
            base_path = strdup(argv[i]);

            //base_path = strdup("./");
            if(follow_symbolic)
            {
                if(stat(base_path, &statbuffer) == -1)
                {
                    perror("Unable to get stat info for input file");
                }
            }
            else
            {
                if(lstat(base_path, &statbuffer) == -1)
                {
                    perror("Unable to get stat info for input file");
                }
            }

            base_dirs[base_dirs_index].path =  base_path;
            base_dirs[base_dirs_index].file_name = dir_path_to_dir_name(base_path);
            base_dirs[base_dirs_index].statbuffer = statbuffer;
            num_base_dirs += 1;
        }
        else
        {
            printf("probably should not be here %s\n",argv[i]);
        }
    }
    if(num_base_dirs == 0)
    {
        struct stat statbuffer;
        char* base_path;
        int base_dirs_index = num_base_dirs;

        // need to fix problem with -L coming before the files.
        base_path = strdup("./");

        //base_path = strdup("./");
        if(follow_symbolic)
        {
            if(stat(base_path, &statbuffer) == -1)
            {
                perror("Unable to get stat info for input file");
            }
        }
        else
        {
            if(lstat(base_path, &statbuffer) == -1)
            {
                perror("Unable to get stat info for input file");
            }
        }

        base_dirs[base_dirs_index].path =  base_path;
        base_dirs[base_dirs_index].file_name = dir_path_to_dir_name(base_path);
        base_dirs[base_dirs_index].statbuffer = statbuffer;
        num_base_dirs += 1;
    }

    printf("----------------end parse--------------------\n");
}


int main(int argc, char** argv)
{

    /* use realpath
    througout for path resolution and link handling.
    */

    // overestimate the number of start dirs so that reallocation will not be
    // necessary.
    base_dirs = (file_data_t*) calloc(argc, sizeof(file_data_t));
    parse_args(argc, argv);
    printf("---------------Calling walkdir--------------\n");
    //system("grep percent findman.txt > output.txt");
    for (int i = 0; i < num_base_dirs; i++)
    {
        walk_dir(base_dirs[i]);
    }
    return 0;
}