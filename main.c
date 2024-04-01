#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <dirent.h>
#include "text_format.h"

#define FALSE 0
#define TRUE 1
#define BOOL u_int8_t
#define INPUT_BUFF_SIZE 32
#define PARSE_BUFF_SIZE 16
#define BUFF_FILES_SIZE 12
#define delim " "

struct file_info_st {
    char* name;
    int type;
};
typedef struct file_info_st file_info;

void sh_loop();
char* get_input();
char** parse_args(char*);
int32_t run_command(char**);
int32_t tsh_launch(char** args);
char* get_curr_dir();
int32_t cmp_args(char** args,char** cmp);
void load_settings(unsigned char*symo);
int32_t set_settings();
int32_t set_in_format(FILE* f);
int32_t set_out_format(FILE* f);
int32_t set_symbol(FILE* f);
file_info* read_files_info(DIR* d,int* c);
file_info* sort_files_name(file_info* buff_files,int32_t c,BOOL a);

int32_t tsh_cd(char** args);
int32_t tsh_help(char** args);
int32_t tsh_exit(char** args);
int32_t tsh_ls(char** args);
void tsh_ls_help();
int32_t ch_set(char** args);

int32_t (*builtins[])(char**) = {
        tsh_cd,
        tsh_help,
        tsh_exit,
        tsh_ls,
        ch_set
};

char* builtins_str[] = {"cd", "help", "exit","ls","change_settings"};
int32_t builtins_count() {
    return sizeof(builtins_str)/sizeof(char*);
}

char* in_text_format;
char* out_text_format;
unsigned char symbol;

int main(int argc,char** argv)
{
    sh_loop();
    return 0;
}

// living loop of shell
void sh_loop()
{

    load_settings(&symbol);

    char* string;
    char** args;
    int32_t status;
    char* cwd;
    do {
        cwd = get_curr_dir();
        printf("%s%s%s%c %s%s",STANDART_OUT_FORMAT, out_text_format, cwd, symbol, STANDART_OUT_FORMAT, in_text_format);
        string = get_input();
        printf(STANDART_OUT_FORMAT);
        args = parse_args(string);
        status = run_command(args);
        free(args);
        free(string);
        free(cwd);
    }while(status);
    free(out_text_format);
    free(in_text_format);
}

//return input from user
char* get_input()
{
    int32_t buff_size = INPUT_BUFF_SIZE;
    char* string = (char*)malloc(sizeof(char) * buff_size);
    if(string == NULL) {
        fprintf(stderr, "%s\n%stsh: input string malloc wrong\n%s",STANDART_OUT_FORMAT,RED_TEXT_COLOR,STANDART_OUT_FORMAT);
        exit(EXIT_FAILURE);
    }
    int32_t c, pos = 0;

    c = getc(stdin);
    while(c != EOF && c != '\n') {
        string[pos] = (char)c;
        ++pos;
        if(pos >= buff_size) {
            buff_size += INPUT_BUFF_SIZE;
            char* str = realloc(string,buff_size);
            if(str == NULL) {
                fprintf(stderr, "%s\n%stsh: input string realloc wrong\n%s",STANDART_OUT_FORMAT,RED_TEXT_COLOR,STANDART_OUT_FORMAT);
                exit(EXIT_FAILURE);
            }
            string = str;
        }
        c = getc(stdin);
    }
    string[pos] = '\0';
    return string;
}

//take a line of argument and return array of string arguments divided by delim
char** parse_args(char* string)
{
    int32_t buff_size = PARSE_BUFF_SIZE,pos = 0;
    char** args = (char**)malloc(sizeof(char*) * buff_size);
    if(args == NULL) {
        fprintf(stderr,"%s\n%spars args malloc failed\n%s",STANDART_OUT_FORMAT,RED_TEXT_COLOR,STANDART_OUT_FORMAT);
        exit(EXIT_FAILURE);
    }
    char* str;

    str = strtok(string,delim);
    while(str != NULL) {
        args[pos] = str;
        ++pos;
        if (pos >= buff_size) {
            buff_size += PARSE_BUFF_SIZE;
            args = realloc(args, sizeof(char *) * buff_size);
            if (args == NULL)
                exit(EXIT_FAILURE);
        }
        str = strtok(NULL, delim);
    }
    args[pos] = NULL;

    return args;
}

//execute command which consists in args[0] and give args from args[1]
int32_t run_command(char** args)
{
    if(args[0] == NULL)
        return 1;
    for (int i = 0; i < builtins_count(); ++i) {
        if(strcmp(builtins_str[i], args[0]) == 0)
            return (builtins[i])(&args[1]);
    }
    return tsh_launch(args);
}

//create new process to call not builtin function
int32_t tsh_launch(char** args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();
    if(pid == 0) {
        if(execvp(args[0],args) == -1)
            perror(STANDART_OUT_FORMAT"tsh");
        exit(EXIT_FAILURE);
    } else if(pid < 0) {
        perror(STANDART_OUT_FORMAT"tsh");
    } else {
        do {
            wpid = waitpid(pid,&status,WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

//return path to current directory
char* get_curr_dir()
{
    return getcwd(NULL,0);
}

/* take:
 * args - arguments which should be compared. Should have NULL in last element
 * cmp  - list of arguments which has call function. Should have NULL in last element
 *
 * compare string from args and cmp, return value all bits which is result of compare strings
 * first (at right) bit equal result of compare cmp[0] and args[j]
 * second (at right) bit equal result of compare cmp[1] and args[j]
 *
 *  return mask of flags where most right bit is result of compare cmp[0]
 *  if argument from args not exists return index of this argument multiplied by -1
 */
int32_t cmp_args(char** args,char** cmp)
{
    BOOL exist;
    int32_t res = 0;
    for(int i = 0;args[i] != NULL;++i) {
        exist = FALSE;
        for(int j = 0;cmp[j] != NULL;++j) {
            if(strcmp(args[i],cmp[j]) == 0) {
                res = res | (0b1 << j);
                exist = TRUE;
                break;
            }
        }
        if(!exist)
            return i*(-1);
    }

    return res;
}

// load settings from file "tsh_settings.txt"
void load_settings(unsigned char*symo)
{
    FILE* f;
    char* str;
    while (1) {
        if((f = fopen("tsh_settings.txt","r")) == NULL) {
            printf("\ntsh: file with setting not fined, let's set new settings\n");
            set_settings();
        }
        size_t SIZE = 0;
        char* line = NULL;
        while (getline(&line, &SIZE, f) > 0) {
            str = strtok(line, delim);
            do {
                if (strcmp(str, "in_s") == 0) {
                    str = strtok(NULL, delim);
                    in_text_format = (char *) malloc(sizeof(str));
                    in_text_format = strcpy(in_text_format, str);
                } else if (strcmp(str, "out_s") == 0) {
                    str = strtok(NULL, delim);
                    out_text_format = (char *) malloc(sizeof(str));
                    out_text_format = strcpy(out_text_format, str);
                } else if (strcmp(str, "sym") == 0)
                    *symo = strtok(NULL, delim)[0];

            } while ((str = strtok(NULL, delim)) != NULL);
        }
        if (in_text_format == NULL) {
            set_settings();
            continue;
        }
        break;
    }
    int a = sizeof(in_text_format);
    in_text_format[strlen(in_text_format) - 1] = '\0';
    out_text_format[strlen(out_text_format) - 1] = '\0';
    fclose(f);
}

//create file "tsh_setting.txt" and write settings
int32_t set_settings()
{
    FILE* f;
    if((f = fopen("tsh_settings.txt","w")) == NULL) {
        fprintf(stderr, "%s\ntsh: cant create file with settings\n%s",RED_TEXT_COLOR,STANDART_OUT_FORMAT);
        return 0;
    }

    set_in_format(f);
    set_out_format(f);
    set_symbol(f);

    fclose(f);
    return 1;
}

// get input from user and write shell input text format in file
int32_t set_in_format(FILE* f)
{
    size_t SIZE = 2;
    char* str = (char*)malloc(sizeof(char) * SIZE);
    if(str == NULL) {
        fprintf(stderr, "%s\ntsh: bad alloc in set_settings\n%s", RED_TEXT_COLOR, STANDART_OUT_FORMAT);
        return -1;
    }

    fprintf(f,"in_s ");
    while (1) {
        printf("\nWhich color of text input you like to see?\n1)"WHITE_TEXT_COLOR"White"STANDART_OUT_FORMAT"\t\t2)"
               BLACK_TEXT_COLOR"Black"STANDART_OUT_FORMAT"\t\t3)"
               BLUE_TEXT_COLOR"Blue"STANDART_OUT_FORMAT
               "\n4)"
               RED_TEXT_COLOR"Red"STANDART_OUT_FORMAT
               "\t\t5)"
               GREEN_TEXT_COLOR"Green"STANDART_OUT_FORMAT
               "\t\t6)"
               YELLOW_TEXT_COLOR"Yellow"STANDART_OUT_FORMAT
               "\t\t7)Default"
               "\n> ");
        int count = getline(&str,&SIZE,stdin);
        if(count > 2)
            continue;
        switch (str[0]) {
            case '1':
                fprintf(f,WHITE_TEXT_COLOR);
                break;
            case '2':
                fprintf(f,BLACK_TEXT_COLOR);
                break;
            case '3':
                fprintf(f,BLUE_TEXT_COLOR);
                break;
            case '4':
                fprintf(f,RED_TEXT_COLOR);
                break;
            case '5':
                fprintf(f,GREEN_TEXT_COLOR);
                break;
            case '6':
                fprintf(f,YELLOW_TEXT_COLOR);
                break;
            case '7':
                fprintf(f,STANDART_OUT_FORMAT);
                break;
            default:
                fprintf(stderr,"\ntsh: You entered smth different from digit. Lets try again!\n");
                continue;
        }
        break;
    }
    while (1) {
        printf("\nWhich color of text input background you like to see?\n1)"
               WHITE_TEXT_BACK_COLOR"White"STANDART_OUT_FORMAT"\t\t2)"
               BLACK_TEXT_BACK_COLOR"Black"STANDART_OUT_FORMAT"\t\t3)"
               BLUE_TEXT_BACK_COLOR"Blue"STANDART_OUT_FORMAT
               "\n4)"
               RED_TEXT_BACK_COLOR"Red"STANDART_OUT_FORMAT
               "\t\t5)"
               GREEN_TEXT_BACK_COLOR"Green"STANDART_OUT_FORMAT
               "\t\t6)"
               YELLOW_TEXT_BACK_COLOR"Yellow"STANDART_OUT_FORMAT
               "\t\t7)Default"
               "\n> ");
        int count = getline(&str,&SIZE,stdin);
        if(count > 2)
            continue;
        switch (str[0]) {
            case '1':
                fprintf(f,WHITE_TEXT_BACK_COLOR"\n");
                break;
            case '2':
                fprintf(f,BLACK_TEXT_BACK_COLOR"\n");
                break;
            case '3':
                fprintf(f,BLUE_TEXT_BACK_COLOR"\n");
                break;
            case '4':
                fprintf(f,RED_TEXT_BACK_COLOR"\n");
                break;
            case '5':
                fprintf(f,GREEN_TEXT_BACK_COLOR"\n");
                break;
            case '6':
                fprintf(f,YELLOW_TEXT_BACK_COLOR"\n");
                break;
            case '7':
                fprintf(f,"\n");
                break;
            default:
                fprintf(stderr,"\ntsh: You entered smth different from digit. Lets try again!\n");
                continue;
        }
        break;
    }
    return 1;
}

// get input from user and write shell output text format in file
int32_t set_out_format(FILE* f)
{
    size_t SIZE = 2;
    char* str = (char*)malloc(sizeof(char) * SIZE);
    if(str == NULL) {
        fprintf(stderr, "%s\ntsh: bad alloc in set_settings\n%s", RED_TEXT_COLOR, STANDART_OUT_FORMAT);
        return -1;
    }

    fprintf(f,"out_s ");
    while (1) {
        printf("\nWhich color of text output you like to see?\n1)"
               WHITE_TEXT_COLOR"White"STANDART_OUT_FORMAT"\t\t2)"
               BLACK_TEXT_COLOR"Black"STANDART_OUT_FORMAT"\t\t3)"
               BLUE_TEXT_COLOR"Blue"STANDART_OUT_FORMAT
               "\n4)"
               RED_TEXT_COLOR"Red"STANDART_OUT_FORMAT
               "\t\t5)"
               GREEN_TEXT_COLOR"Green"STANDART_OUT_FORMAT
               "\t\t6)"
               YELLOW_TEXT_COLOR"Yellow"STANDART_OUT_FORMAT
               "\t\t7)Default"
               "\n> ");
        int count = getline(&str,&SIZE,stdin);
        if(count > 2)
            continue;
        switch (str[0]) {
            case '1':
                fprintf(f,WHITE_TEXT_COLOR);
                break;
            case '2':
                fprintf(f,BLACK_TEXT_COLOR);
                break;
            case '3':
                fprintf(f,BLUE_TEXT_COLOR);
                break;
            case '4':
                fprintf(f,RED_TEXT_COLOR);
                break;
            case '5':
                fprintf(f,GREEN_TEXT_COLOR);
                break;
            case '6':
                fprintf(f,YELLOW_TEXT_COLOR);
                break;
            case '7':
                fprintf(f,STANDART_OUT_FORMAT);
                break;
            default:
                fprintf(stderr,"\ntsh: You entered smth different from digit. Lets try again!\n");
                continue;
        }
        break;
    }

    while (1) {
        printf("\nWhich color of text output background you like to see?\n1)"
               WHITE_TEXT_BACK_COLOR"White"STANDART_OUT_FORMAT"\t\t2)"
               BLACK_TEXT_BACK_COLOR"Black"STANDART_OUT_FORMAT"\t\t3)"
               BLUE_TEXT_BACK_COLOR"Blue"STANDART_OUT_FORMAT
               "\n4)"
               RED_TEXT_BACK_COLOR"Red"STANDART_OUT_FORMAT
               "\t\t5)"
               GREEN_TEXT_BACK_COLOR"Green"STANDART_OUT_FORMAT
               "\t\t6)"
               YELLOW_TEXT_BACK_COLOR"Yellow"STANDART_OUT_FORMAT
               "\t\t7)Default"
               "\n> ");
        int count = getline(&str,&SIZE,stdin);
        if(count > 2)
            continue;
        switch (str[0]) {
            case '1':
                fprintf(f,WHITE_TEXT_BACK_COLOR"\n");
                break;
            case '2':
                fprintf(f,BLACK_TEXT_BACK_COLOR"\n");
                break;
            case '3':
                fprintf(f,BLUE_TEXT_BACK_COLOR"\n");
                break;
            case '4':
                fprintf(f,RED_TEXT_BACK_COLOR"\n");
                break;
            case '5':
                fprintf(f,GREEN_TEXT_BACK_COLOR"\n");
                break;
            case '6':
                fprintf(f,YELLOW_TEXT_BACK_COLOR"\n");
                break;
            case '7':
                fprintf(f,"\n");
                break;
            default:
                fprintf(stderr,"\ntsh: You entered smth different from digit. Lets try again!\n");
                continue;
        }
        break;
    }
    return 1;
}

// get input from user and write shell promt for input in file
int32_t set_symbol(FILE* f)
{
    size_t SIZE = 2;
    char* str = (char*)malloc(sizeof(char) * SIZE);
    if(str == NULL) {
        fprintf(stderr, "%s\ntsh: bad alloc in set_settings\n%s", RED_TEXT_COLOR, STANDART_OUT_FORMAT);
        return -1;
    }

    while(1) {
        printf("\nwhich symbol in invite to enter you like to see\n");
        int count = getline(&str,&SIZE,stdin);
        if(count > 2) {
            printf("\ntsh: you entered more then 1 symbol. Lets try again\n");
            continue;
        }
        fprintf(f,"sym %c\n",str[0]);
        break;
    }
    return 1;
}

//read information about files contained in d directory and return pointer to array file_info type
file_info* read_files_info(DIR* d,int* c)
{
    int pos = 0;
    struct dirent* dir;
    int32_t buff_files_sz = BUFF_FILES_SIZE;
    file_info* buff_files = (file_info*)malloc(sizeof(file_info) * buff_files_sz);
    if(d == NULL)
        d = opendir(get_curr_dir());
    if(buff_files == NULL)
        return NULL;
    while ((dir = readdir(d)) != NULL) {
        if(pos >= buff_files_sz) {      // realloc memory if its end
            buff_files_sz += BUFF_FILES_SIZE;
            buff_files = (file_info*) realloc(buff_files,buff_files_sz);
            if(buff_files == NULL) {
                fprintf(stderr,"%s%s\nls: Alloc wrong, out not sorted\n%s",STANDART_OUT_FORMAT,RED_TEXT_COLOR,STANDART_OUT_FORMAT);
                break;
            }
        }
        buff_files[pos].name = dir->d_name;
        buff_files[pos].type = dir->d_type;
        ++pos;
    }
    *c = pos;
    return buff_files;
}

// sort files by name and return poinet to buff_files
// buff_files - source of files
// c - count of elements
// if a is true sort by alfabet, else otherwise conversely
file_info* sort_files_name(file_info* buff_files,int32_t c,BOOL a)
{
    if(a) {
        for (int i = 0; i < c; ++i) {
            for (int j = 0; j < c; ++j) {
                if(strcmp(buff_files[i].name, buff_files[j].name) < 0) {
                    file_info temp = buff_files[i];
                    buff_files[i] = buff_files[j];
                    buff_files[j] = temp;
                }
            }
        }
    }
    else {
        for (int i = 0; i < c; ++i) {
            for (int j = 0; j < c; ++j) {
                if(strcmp(buff_files[i].name, buff_files[j].name) > 0) {
                    file_info temp = buff_files[i];
                    buff_files[i] = buff_files[j];
                    buff_files[j] = temp;
                }
            }
        }
    }
}

int32_t tsh_cd(char** args)
{
    if(args[0] == NULL) {
        fprintf(stderr,"%s\n%scd: unexpected arguments! expected path\n%s",STANDART_OUT_FORMAT,RED_TEXT_COLOR,STANDART_OUT_FORMAT);
        return 1;
    }
    if(chdir(args[0]) != 0)
        printf("%s\n%scd: not such file in directory\n%s",STANDART_OUT_FORMAT,out_text_format, STANDART_OUT_FORMAT);
    return 1;
}

int32_t tsh_help(char** args)
{
    if(*args == NULL) {
        printf("%s\n%sHello in my TIMA SHELL!%s\n",STANDART_OUT_FORMAT,out_text_format,STANDART_OUT_FORMAT);
        printf("%sExists functions:%s\n",out_text_format,STANDART_OUT_FORMAT);
        for (int32_t i = 0; i < builtins_count(); ++i) {
            printf("| %s |", builtins_str[i]);
        }
        printf(STANDART_OUT_FORMAT"\n");
        printf("%sif u want get information about builtin functions write help func_name%s\n",out_text_format,STANDART_OUT_FORMAT);
    }
    else if(args[1] != NULL) {
        printf("\n%stsh: is too much arguments%s\n", out_text_format, STANDART_OUT_FORMAT);
    }
    else {
        args--; // args pointer was increased from run_command
        args[0] = args[1];
        args[1] = "-h";
        run_command(args);
    }

    return 1;
}

int32_t tsh_exit(char** args)
{
    return 0;
}

int32_t tsh_ls(char** args) {
#include "ls_macros.h" // included here in order to limit the scope
    char* exist_args[] = {"-all","-a","-dir","-d","-file","-f","-nsort","-S","-help","-h",NULL};

    int32_t flags = cmp_args(args,exist_args);
    if(flags <= 0) {
        printf("%s\n%sls: %s is not correct argument%s\n",STANDART_OUT_FORMAT,out_text_format, args[-flags], STANDART_OUT_FORMAT);
        return 1;
    }
    if(flags & TLS_HELP) {
        tsh_ls_help();
        return 1;
    }

    DIR* d = opendir(get_curr_dir());
    struct dirent* dir;
    int32_t count = 0;  // output file counter
    file_info* buff_files;

    if(buff_files != NULL && !(flags & TLS_NSORT)) { // sort if its need
        int pos = 0;
        buff_files = read_files_info(d,&pos);
        if(buff_files != NULL) {
            sort_files_name(buff_files,pos,1);

            for (int i = 0; i < pos; ++i) {             //output
                if (buff_files[i].type == DT_DIR && ((flags & TLS_ALL) || (flags & TLS_DIR))) {
                    printf("%s%s%s\n", BLUE_TEXT_COLOR, buff_files[i].name, STANDART_OUT_FORMAT);
                    ++count;
                }
                else if (buff_files[i].type == DT_REG && ((flags & TLS_ALL) || (flags & TLS_FILE))) {
                    printf("%s%s%s\n", BIRUZ_TEXT_COLOR, buff_files[i].name, STANDART_OUT_FORMAT);
                    ++count;
                }
            }
            free(buff_files);
        }
    }
    if(buff_files == NULL || (flags & TLS_NSORT)) {         //output if bad alloc or -S argument
        while ((dir = readdir(d)) != NULL) {
            if (((flags & TLS_ALL) || (flags & TLS_DIR)) && dir->d_type == DT_DIR)
                printf("%s%s%s\n", BLUE_TEXT_COLOR, dir->d_name, STANDART_OUT_FORMAT);
            else if (((flags & TLS_ALL) || (flags & TLS_FILE)) && dir->d_type == DT_REG)
                printf("%s%s%s\n", BIRUZ_TEXT_COLOR, dir->d_name, STANDART_OUT_FORMAT);
            ++count;
        }
    }

    printf("total: %d\n",count);
    closedir(d);
    return 1;
}

void tsh_ls_help()
{
    printf("\n%sOutput files and directories%s\n"
           BOLD_TEXT_STYLE"\n\n%sOPTIONS:%s\n\n"
           BOLD_TEXT_STYLE "%s-all,\t-a" STANDART_OUT_FORMAT "%s:\toutput all files and directories%s\n" // this 1 line
           BOLD_TEXT_STYLE "%s-dir,\t-d" STANDART_OUT_FORMAT "%s:\toutput only directories%s\n"
           BOLD_TEXT_STYLE "%s-file,\t-f" STANDART_OUT_FORMAT "%s:\toutput only files%s\n"
           BOLD_TEXT_STYLE "%s-nsort,\t-S" STANDART_OUT_FORMAT "%s:\tnot sort file names in output%s\n"
           BOLD_TEXT_STYLE "%s-help,\t-h" STANDART_OUT_FORMAT "%s:\tallows get useful information%s\n\n",
           out_text_format,STANDART_OUT_FORMAT,out_text_format,STANDART_OUT_FORMAT,
           out_text_format,out_text_format,STANDART_OUT_FORMAT, // 1 line format
           out_text_format,out_text_format,STANDART_OUT_FORMAT,
           out_text_format,out_text_format,STANDART_OUT_FORMAT,
           out_text_format,out_text_format,STANDART_OUT_FORMAT,
           out_text_format,out_text_format,STANDART_OUT_FORMAT
    );
}

int32_t ch_set(char** args)
{
    set_settings();
    load_settings(&symbol);
    return 1;
}