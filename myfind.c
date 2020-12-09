/*

    Project 4: find
    Completed by: Kai Pinckard

*/
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
#include <ctype.h>

const int NUM_SECS_PER_DAY = 86400;
// For -name, by default not used
char* pattern = NULL;
// For -mtime, by default num_days is not used
int num_days = -1;
// Stores all modes that will be accepted.
mode_t* desired_modes = NULL;
int num_modes = 0;
// This stores the arguments to -exec
char* exec_args = NULL;
// By default assume the user has not specified -print
// This does not mean that nothing is printed
bool should_print = false;
// By default do not follow symbolic links
bool follow_symbolic = false;
// This is used to store the order arguments occur in.
char** opt_order = NULL;
// This is used to keep track of the length of opt_order
int opt_order_len = 0;

// A convenient structure to hold file data.
typedef struct 
{
    // Store the file path
    // for example ../testdir/file.txt
    // or ./subdir/subsubdir/
    char* path;
    // Store the file_name
    // for example file.txt
    // subsubdir
    char* file_name;
    // stores the information from calling stat
    struct stat statbuffer;

} file_data_t;


// These are needed to keep track of the original base dirs
// specified by the user. They are stored globally because
// base dirs should ignore normal formatting and be printed with
// the original formatting the user used. (ie if "/" was used or not at the end).

int num_base_dirs = 0;
file_data_t* base_dirs = NULL;

/*
    Returns a new char* containing the directory with the last '/' removed if present
    For example ../exampledir/ becomes ../exampledir. Caller is responsible for 
    deallocating returned char*. Assumes directory is not NULL.
*/
char* remove_last_slash(const char* directory)
{
    if(strlen(directory) > 0)
    {
        int last_char_index = strlen(directory) - 1;
        if(directory[last_char_index] == '/')
        {
            char* dir_no_slash = strndup(directory, last_char_index);
            if(dir_no_slash == NULL)
            {
                printf("find: insufficient memory\n");
                exit(1);
            }
            return dir_no_slash;
        }
    }
    char* dir_no_slash = strdup(directory);
    if(dir_no_slash == NULL)
    {
        printf("find: insufficient memory\n");
        exit(1);
    }
    return dir_no_slash;
}

/*
    Returns the dir_name in a new char* caller is responsible for deallocating.
    For example ../dir/exampledir/ becomes exampledir
*/
char* dir_path_to_dir_name(const char* directory)
{
    char* dir_path_no_slash = remove_last_slash(directory);

    // get pointer to last occurance of char
    char* start_pos = strrchr(dir_path_no_slash, '/');
    if(start_pos != NULL)
    {
        // copy str starting from the next char on.
        char* dir_name = strdup(start_pos + 1);
        if(dir_name == NULL)
        {
            printf("find: insufficient memory\n");
            exit(1);
        }
        free(dir_path_no_slash);
        return dir_name;
    }
    return dir_path_no_slash;
}

/*
    Returns the base_dir that corresponds to path or NULL if path
    is not a base_dir. A path corresponds to a base_dir if the two are the 
    same ignoring the presence of a trailing slash. 
*/
char* matching_base_dir(const char* path)
{
    char* new_path = remove_last_slash(path);
    for (int i = 0; i < num_base_dirs; i++)
    {
        if(base_dirs[i].path != NULL)
        {
            char* new_base = remove_last_slash(base_dirs[i].path);
            if(strcmp(new_base, new_path) == 0)
            {
                free(new_base);
                free(new_path);
                char* matching_base_dir = strdup(base_dirs[i].path);
                if(matching_base_dir == NULL)
                {
                    printf("find: insufficient memory\n");
                    exit(1);
                }
                return matching_base_dir;
            }
            free(new_base);
        }
    }
    free(new_path);
    return NULL;
}

/*
    Takes a complete path as an input such as ../testdir/dir/file.txt or ../testdir/otherdir/
    prints out the path minus the last slash if it is present. However, base dirs are be printed as specified by the
    user and not following the usual convention.
*/
void print_match(const char* path)
{
    int length = strlen(path);

    // If the path corresponds to a base dir then print it as originally
    // specified by the user

    char* match;
    if( (match = matching_base_dir(path) ) != NULL)
    {
        printf("%s\n", match);
        free(match);
        return;
    }

    // If the path is not a base dir and ends in a slash do not print the slash.
    if(strlen(path) > 0)
    {
        if(path[length - 1] == '/')
        {
            length--;
        }
    }

    for (int i = 0; i < length; i++)
    {
        putchar(path[i]);
    }
    putchar('\n');
}

/*
    Returns true if the file_name matches the pattern. The pattern
    is the value passed to -name
*/
bool handle_name(const char* pattern, const char* file_name)
{
    return fnmatch(pattern, file_name, 0) == 0;
}

/*
    Returns true if the modified time is within num_days of the
    current time.
*/
bool occurred_within(const time_t current_time, const time_t m_time, const int num_days)
{
    
    long int max_dif = NUM_SECS_PER_DAY * (num_days + 1);
    long int min_dif = NUM_SECS_PER_DAY * num_days;
    time_t time_dif = current_time - m_time;
    return time_dif < max_dif && time_dif > min_dif;
}

/*
    Returns true if the file specified by file_name was modified at
    mtime. Note +/- features not implemented. 
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
    // If -type was not specified
    if(num_modes == 0)
    {
        return (file.statbuffer.st_mode & S_IFMT) == S_IFMT;
    }
    else
    {
        // return true if the type matches any of the accepted types.
        for(int i = 0; i < num_modes; i++)
        {
            if((file.statbuffer.st_mode & S_IFMT) == desired_modes[i])
            {
                return true;
            }
        }
        return false;
    }
}

/* 
    Populate the brackets in exec_args with the file path and return a new string.
    Caller is responsible for deallocating the returned string.
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
    if(filled_exec_args == NULL)
    {
        printf("find: insufficient memory\n");
        exit(1);
    }

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
    return filled_exec_args;
}

/*
    Executes the specified command on the file specified. Returns true if
    the command succeeded and false otherwise.
*/
bool handle_exec(file_data_t file)
{
    char* filled_exec_args = populate_command(file);
    bool return_value = system(filled_exec_args) == 0;
    free(filled_exec_args);
    return return_value;
}

/*
    Checks if a file matches all the requirements specified by the options
    then either executes a command on it or prints it out as required by the
    specified options.
*/
void handle_file(const file_data_t file)
{
    // used to check if default printing behavior should kick in.
    bool already_printed = false;

    // handle the options in the order they were passed in.
    for(int i = 0; i < opt_order_len; i++)
    {
        if(strcmp(opt_order[i], "-name") == 0)
        {
            if(pattern != NULL && !handle_name(pattern, file.file_name))
            {
                return;
            }
        }
        else if(strcmp(opt_order[i], "-mtime") == 0)
        {
            if(num_days != -1 && !handle_mtime(file))
            { 
                return;
            }
        }
        else if(strcmp(opt_order[i], "-type") == 0)
        {
            if(num_modes > 0 && !handle_type(file))
            {
                return;
            }
        }
        else if(strcmp(opt_order[i], "-exec") == 0)
        {
            if(exec_args != NULL && !handle_exec(file))
            {
                return;
            } 
        }
        else if(strcmp(opt_order[i], "-print") == 0)
        {
            if(exec_args == NULL || should_print)
            {
                print_match(file.path);
            }
            already_printed = true;
        }
    }
    if( (already_printed == false) && (exec_args == NULL || should_print) )
    {
        print_match(file.path);
    }   
}

/*
    Returns a new string that the caller must deallocate that contains a trailing
    slash.
*/
char* add_slash(const char* directory)
{
    int length = strlen(directory) + 2;
    char* dir = (char*) calloc(length, sizeof(char));
    if(dir == NULL)
    {
        printf("find: insufficient memory\n");
        exit(1);
    }
    return strcat(strcat(dir, directory), "/");
}

/*
    Returns a new char* containing the directory and the file name
    joined into a valid file path. Caller is responsible for deallocating
    the returned char*.
*/
char* join_path(const char* directory, const char* file_name)
{
    int dir_len = strlen(directory);
    int total_length = dir_len + strlen(file_name) + 1;
    bool needs_slash = directory[dir_len-1] != '/';

    if (needs_slash)
    {
        total_length++;
    }

    char* path = (char*) calloc(total_length, sizeof(char));
    if(path == NULL)
    {
        printf("find: insufficient memory\n");
        exit(1);
    }
    strcat(path, directory);

    if(needs_slash)
    {
        strcat(path, "/");
    }

    return strcat(path, file_name);
}

/*
    Works like stat or lstat depending on value of follow_symbolic
    returns true if able to get stat info false otherwise.
*/
bool get_stat_info(const char* path,  struct stat* statbuffer)
{
    if(follow_symbolic)
    {
        if(stat(path, statbuffer) == -1)
        {
            return false;
        }
    }
    else
    {
        if(lstat(path, statbuffer) == -1)
        {
            return false;
        }
    }
    return true;
}

/*
    Recursively traverses the specified directory calling handle_file on each
    file or directory in encountered during the traversal.
*/
void walk_dir(file_data_t dir_file_data)
{
    struct dirent *de;
    
    // Every directory handles it self at the beginning
    handle_file(dir_file_data);

    DIR* dr = opendir(dir_file_data.path);

    // if dir_file_data is not a directory or can not be opened skip it
    if (dr == NULL)
    {
        char* error_path = remove_last_slash(dir_file_data.path);
        printf("find: ‘%s’: Permission denied\n", error_path);
        free(error_path);
        free(dir_file_data.path);
        free(dir_file_data.file_name);
        return;
    }

    // Get each of the directory entries and handle them as well.
    while((de = readdir(dr)) != NULL)
    {
        // Check that the current item is not . or ..
        if ( (strcmp(de->d_name, "..") != 0 ) && (strcmp(de->d_name, ".") != 0) )
        {
            char* file_name = strdup(de->d_name);
            if(file_name == NULL)
            {
                printf("find: insufficient memory\n");
                exit(1);
            }
            char* path = join_path(dir_file_data.path, de->d_name);

            struct stat statbuffer;
            get_stat_info(path, &statbuffer);
            file_data_t cur_file = {path, file_name, statbuffer};

            // Check if the file is of type directory
            if((cur_file.statbuffer.st_mode & S_IFMT) == S_IFDIR)
            {
                // Only follow symbolic links if specified
                if((cur_file.statbuffer.st_mode & S_IFMT) != S_IFLNK || follow_symbolic)
                {
                    char* new_path = add_slash(cur_file.path);
                    free(cur_file.path);
                    cur_file.path = new_path;
                    // Call this function again on each sub directory
                    walk_dir(cur_file);
                }
            }
            else
            {
                // No subdirectories are handled here
                handle_file(cur_file);
                free(cur_file.path);
                free(cur_file.file_name);
            }
        }
    }
    free(dir_file_data.path);
    free(dir_file_data.file_name);
    free(de);
    closedir(dr);
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
        default: 
            printf("find: Unknown argument to -type: %c\n", mode_specifier);
            exit(1);
    }
}

/*
    Parses the arguments to -type. Stores the results in global 
    desired_modes and updates num_modes to the correct number.
*/
void arg_to_mode(char** argv, int index)
{
    // Holds the string the mode specifications.
    char* mode_specifier = argv[index+1];
    int length = strlen(mode_specifier);
   
    // Reserve space storing modes.
    desired_modes = (mode_t*) calloc(length % 2 + length / 2, sizeof(mode_t));
    if(desired_modes == NULL)
    {
        printf("find: insufficient memory\n");
        exit(1);
    }

    // Keep track of if we are expecting another mode.
    bool needs_mode = false;
    for(int i = 0; i < length; i++)
    {
        // if i is odd then it must be a ','
        if(i % 2 == 1)
        {
            // We are expecting another argument to -type since there was a preceeding ','
            needs_mode = true;
            if(mode_specifier[i] != ',')
            {
                printf("find: Must separate multiple arguments to -type using: ','\n");
                exit(1);
            }
        }
        else
        {
            needs_mode = false;
            desired_modes[num_modes] = get_mode_mask(mode_specifier[i]);
            num_modes += 1;
        }
    }
    if(needs_mode)
    {
        printf("find: Last file type in list argument to -type is missing, i.e., list is ending on: ','\n");
        exit(1);
    }
}

/*
    parses the exec commands starting at index index and updates index to hold
    the the index of the semicolon
*/
void get_exec_args(char** argv, int argc, int* index)
{

    int semicolon_index = -1; 

    // start at one so we dont have to add space for terminating char later.
    int exec_args_len = 1;
    for(int i = *index + 1; i < argc; i++)
    {
        if (argv[i][0] == ';' && argv[i][1] == '\0')
        {
            semicolon_index = i; 
            break;
        }
        else
        {
            // add space for the argument and a space
            exec_args_len += strlen(argv[i]) + 1;
        }
    }
    exec_args = (char*) calloc(exec_args_len, sizeof(char));
    if(exec_args == NULL)
    {
        printf("find: insufficient memory\n");
        exit(1);
    }
    for(int i = *index + 1; i < semicolon_index; i++)
    {
        strcat(strcat(exec_args, argv[i]), " ");
    }
    if(semicolon_index == -1)
    {
        printf("find: missing argument to `-exec'\n");
        exit(1);
    }
    else
    {
        *index = semicolon_index;
    }
    return;
}

/*
    adds another base dir to the global base_dirs
*/
void base_path_to_file_data(char* base_path)
{
    struct stat statbuffer;
    char* base_path_no_slash = remove_last_slash(base_path);
    if(get_stat_info(base_path, &statbuffer))
    {
        base_dirs[num_base_dirs].path = base_path;
        base_dirs[num_base_dirs].file_name = dir_path_to_dir_name(base_path);
        base_dirs[num_base_dirs].statbuffer = statbuffer;
    }
    else
    {
        // create invalid base dir error messages
        base_dirs[num_base_dirs].path = NULL;
        char* error_begin = "find: ‘";
        char* error_end;

        if(get_stat_info(base_path_no_slash, &statbuffer))
        {
            // Error message for not a directory.
            error_end = "’: Not a directory";
        }
        else
        {
            // Error message for no such file or directory
            error_end = "’: No such file or directory";
        }

        int filled_template_length = strlen(error_begin) + strlen(error_end) + strlen(base_path) + 1;
        char* invalid_dir_message = (char*) calloc(filled_template_length, sizeof(char));
        if(invalid_dir_message == NULL)
        {
            printf("find: insufficient memory\n");
            exit(1);
        }
        strcat(strcat(strcat(invalid_dir_message, error_begin), base_path), error_end);
        
        // for invalid base dirs the file_name stores the error message
        base_dirs[num_base_dirs].file_name = invalid_dir_message;
        free(base_path);
    }

    free(base_path_no_slash);
    num_base_dirs += 1;
}

/*
    Checks if the arg isdigit(). If it is, the numeric
    value is extracted with atoi and stored in num_days.
*/
void parse_mtime(char* arg)
{
    for(long unsigned int i = 0; i < strlen(arg); i++)
    {
        if(!isdigit(arg[i]))
        {
            printf("find: invalid argument `%s' to `-mtime'\n", arg);
            exit(1);
        }
    }
    num_days = atoi(arg);
}

/*
    Parses all of the arguments to myfind. Most are stored as global variables as they change
    the behavior of the entire program. 
*/
void parse_args(int argc, char** argv)
{
    bool more_start_dirs = true;

    // allocate space to store the order of the options. this is an overestimate
    opt_order = (char**) calloc(argc, sizeof(char*));

    char* prev_option;
    for(int i = 1; i < argc; i++)
    {
        // Check for -L first since we don't want to interpret it as a regular option.
        if(strcmp(argv[i], "-L") == 0)
        {
            opt_order[opt_order_len] = strdup("-L");
            prev_option = opt_order[opt_order_len];
            opt_order_len += 1;
            if(prev_option == NULL)
            {
                printf("find: insufficient memory\n");
                exit(1);
            }
            follow_symbolic = true;
        }
        // Check the current arg is an option
        else if(argv[i][0] == '-')
        {
            more_start_dirs = false;

            if(strcmp(argv[i], "-name") == 0)
            {
                opt_order[opt_order_len] = strdup("-name");
                prev_option = opt_order[opt_order_len];
                opt_order_len += 1;
                if(prev_option == NULL)
                {
                    printf("find: insufficient memory\n");
                    exit(1);
                }
                if(argv[i+1] == NULL)
                {
                    printf("find: missing argument to `-name'\n");
                    exit(1);
                }
                pattern = argv[i+1];
                // Increment i to skip parsing the argument to -mtime twice.
                i++;
            }
            else if(strcmp(argv[i], "-mtime") == 0)
            {
                //handle_mtime();
                opt_order[opt_order_len] = strdup("-mtime");
                prev_option = opt_order[opt_order_len];
                opt_order_len += 1;
                if(prev_option == NULL)
                {
                    printf("find: insufficient memory\n");
                    exit(1);
                }
                if(argv[i+1] == NULL)
                {
                    printf("find: missing argument to `-mtime'\n");
                    exit(1);
                }
                parse_mtime(argv[i+1]);
                // Increment i to skip parsing the argument to -mtime twice.
                i++;
            }
            else if(strcmp(argv[i], "-type") == 0)
            {
                //handle_type();
                opt_order[opt_order_len] = strdup("-type");
                prev_option = opt_order[opt_order_len];
                opt_order_len += 1;
                if(prev_option == NULL)
                {
                    printf("find: insufficient memory\n");
                    exit(1);
                }
                if(argv[i+1] == NULL)
                {
                    printf("find: missing argument to `-type'\n");
                    exit(1);
                }
                arg_to_mode(argv, i);
                // Increment i to skip parsing the argument to -type twice.
                i++;
            }
            else if(strcmp(argv[i], "-exec") == 0)
            {
                opt_order[opt_order_len] = strdup("-exec");
                prev_option = opt_order[opt_order_len];
                opt_order_len += 1;
                if(prev_option == NULL)
                {
                    printf("find: insufficient memory\n");
                    exit(1);
                }
                if(argv[i+1] == NULL)
                {
                    printf("find: missing argument to `-exec'\n");
                    exit(1);
                }
                get_exec_args(argv, argc, &i);
                continue;
                
            }
            else if(strcmp(argv[i], "-print") == 0)
            {
                opt_order[opt_order_len] = strdup("-print");
                prev_option = opt_order[opt_order_len];
                opt_order_len += 1;
                if(prev_option == NULL)
                {
                    printf("find: insufficient memory\n");
                    exit(1);
                }
                should_print = true;  
            }
            else
            {
                printf("find: unknown predicate `%s'\n", argv[i]);
                exit(1);
            }
        }
        else if(more_start_dirs)
        {
            char* copy = strdup(argv[i]);
            if(copy == NULL)
            {
                printf("find: insufficient memory\n");
                exit(1);
            }
            base_path_to_file_data(copy);
        }
        else
        {
            printf("find: paths must precede expression: `%s'\n", argv[i]);
            printf("find: possible unquoted pattern after predicate `%s'?\n", prev_option);
        }
    }
    // If no base_dir was specified use "./"
    if(num_base_dirs == 0)
    {
        char* copy = strdup("./");
        if(copy == NULL)
        {
            printf("find: insufficient memory\n");
            exit(1);
        }
        base_path_to_file_data(copy);
    }
}

int main(int argc, char** argv)
{
    base_dirs = (file_data_t*) calloc(argc, sizeof(file_data_t));
    if(base_dirs == NULL)
    {
        printf("find: insufficient memory\n");
        exit(1);
    }
    parse_args(argc, argv);

    for (int i = 0; i < num_base_dirs; i++)
    {
        // Only attempt to walk valid directories.
        if(base_dirs[i].path != NULL)
        {
            file_data_t cur_base_dir;
            if((base_dirs[i].statbuffer.st_mode & S_IFMT) == S_IFDIR)
            {
                cur_base_dir.path = strdup(base_dirs[i].path);
                cur_base_dir.file_name = strdup(base_dirs[i].file_name);
                // check that strdup succeeded
                if(cur_base_dir.path == NULL || cur_base_dir.file_name == NULL)
                {
                    printf("find: insufficient memory\n");
                    exit(1);
                }

                cur_base_dir.statbuffer = base_dirs[i].statbuffer;
                walk_dir(cur_base_dir);
            }
            else
            {
                printf("%s\n", base_dirs[i].path);
            }
        }
        else
        {
            printf("%s\n", base_dirs[i].file_name);
        }
    }

    for (int i = 0; i < num_base_dirs; i++)
    {
        free(base_dirs[i].path);
        free(base_dirs[i].file_name);
    }

    for (int i = 0; i < opt_order_len; i++)
    {
        free(opt_order[i]);
    }

    free(base_dirs);
    free(desired_modes);
    free(exec_args);
    free(opt_order);
    return 0;
}