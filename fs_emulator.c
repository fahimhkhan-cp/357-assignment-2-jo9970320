#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){
     if(argc != 2){
        printf("Invalid input, please type a valid directory\n");
        return 1;
     }
     struct stat direx;
     if (stat(argv[1], &direx) != 0){
        printf("Cannot find dir\n");
        return 1;
     }
     if (S_ISDIR(direx.st_mode) == 0){
        printf("Not a dir\n");
        return 1;
     }
     printf("Dir: %s\n", argv[1]);

     //R2
     if(chdir(argv[1]) != 0){
        printf("Could not find dir\n");
        return 1;
     }

     FILE *file = fopen("inodes_list", "rb");
     if (!file){
        return 1;
     }
    char inode_type[1024] = {0};
     uint32_t inode;
     char type;

     while (fread(&inode,sizeof(uint32_t), 1, file) == 1 && fread(&type,sizeof(char), 1, file) == 1){
        // ignore invalid numbers or types
        if (inode >= 1024 || (type != 'd' && type != 'f')){
            continue;
        }
        inode_type[inode] = type;
        printf("Valid inode: %u (%c)\n", inode, type);
     }
     fclose(file);

     //R3
     int current_inode = 0;
     // check if the root inode exists and is a directory
     if(inode_type[current_inode] != 'd'){
        fprintf(stderr, "Error: inode 0 is not found\n");
        return 1;
     }
     printf("Starting in Inode: %d\n", current_inode);

     //R4
     char line[200];

     while(1){
        printf("> ");
        fflush(stdout);

        //if fgets fails, break out of the while loop
        if(fgets(line, sizeof(line), stdin) == NULL){
            break;
        }
        // fgets adds a trailing newline therefore strcmp won't work 
        // reomve the trailing newline
        line[strcspn(line, "\n")] = '\0';
        if(line[0] == '\0'){
            continue;
        }

        //create two character buffers and initialize to empty to avoid using leftover random data from the memory
        char cmd[200] = {0};
        char arg[200] = {0};

        //split the input line into a command and store how many were successfully read
        int n = sscanf(line, "%199s %199s", cmd, arg);

        //exit 
        if(strcmp(cmd, "exit") == 0){
            if (n != 1) {
                fprintf(stderr, "Error: exit takes no arguments\n");
                continue;
            }
            break;
        }
        //list files in directory
        else if(strcmp(cmd, "ls") == 0){
            if (n != 1) {
                fprintf(stderr, "Error: ls takes no arguments\n");
                continue;
            }
            
            char path[32];
            //convert inode integer into a string
            snprintf(path, sizeof(path), "%d", current_inode);

            //open the file
            FILE *dir = fopen(path, "rb");
            if (!dir) {
                perror("ls fopen");
                continue;
            }

            uint32_t entry_inode;
            char entry_name[32];
            while (fread(&entry_inode, sizeof(uint32_t), 1, dir) == 1 &&
                fread(entry_name, 1, 32, dir) == 32) {
                // make a printable version of the name 
                char printable[33];
                //copy the raw 32-byte filename from the directory into a local buffer
                memcpy(printable, entry_name, 32);

                printable[32] = '\0';
                printf("%u %s\n", entry_inode, printable);
            }

            fclose(dir);
            
        } else if (strcmp(cmd, "cd") == 0) {
            if (n != 2) {
                fprintf(stderr, "Error: cd requires exactly 1 argument\n");
                continue;
            }
            char name[33];
            //copies user provided name
            strncpy(name, arg, 32);
            name[32] = '\0';
            char path[32];
            //convert int to string
            snprintf(path, sizeof(path), "%d", current_inode);

            FILE *dir = fopen(path, "rb");
            if (!dir) {
                perror("cd fopen");
                continue;
            }

            uint32_t entry_inode;
            char entry_name[32];
            int found = 0;

            while (fread(&entry_inode, sizeof(uint32_t), 1, dir) == 1 &&
                fread(entry_name, 1, 32, dir) == 32) {

                char printable[33];
                memcpy(printable, entry_name, 32);
                printable[32] = '\0';

                if (strncmp(printable, name, 32) == 0) {
                    found = 1;
                    break;
                }
            }

            fclose(dir);

            if (!found) {
                fprintf(stderr, "cd: no such directory: %s\n", name);
                continue;
            }

            if (entry_inode >= 1024 || inode_type[entry_inode] != 'd') {
                fprintf(stderr, "cd: not a directory: %s\n", name);
                continue;
            }

            current_inode = entry_inode;

        } else if (strcmp(cmd, "mkdir") == 0) {
            if (n != 2) {
                fprintf(stderr, "Error: mkdir requires exactly 1 argument\n");
                continue;
            }
            char name[33];
            strncpy(name, arg, 32);
            name[32] = '\0';
            char curpath[32];
            snprintf(curpath, sizeof(curpath), "%d", current_inode);

            FILE *curdir = fopen(curpath, "rb");
            if (!curdir) {
                perror("mkdir fopen current dir");
                continue;
            }

            uint32_t e_inode;
            char e_name[32];
            int exists = 0;

            while (fread(&e_inode, sizeof(uint32_t), 1, curdir) == 1 &&
                fread(e_name, 1, 32, curdir) == 32) {

                char printable[33];
                memcpy(printable, e_name, 32);
                printable[32] = '\0';

                if (strncmp(printable, name, 32) == 0) {
                    exists = 1;
                    break;
                }
            }
            fclose(curdir);

            if (exists) {
                fprintf(stderr, "mkdir: entry already exists: %s\n", name);
                continue;
            }

            //initialize to -1 to detect if no free node was found
            int new_inode = -1;
            for (int i = 0; i < 1024; i++) {
                //check if inode slot is unused
                if (inode_type[i] == 0) {
                    new_inode = i;
                    break;
                }
            }
            //check if search failed
            if (new_inode < 0) {
                fprintf(stderr, "mkdir: no free inodes\n");
                continue;
            }

            // create the new directory inode file with '.' and '..'
            char newpath[32];
            snprintf(newpath, sizeof(newpath), "%d", new_inode);

            FILE *newdir = fopen(newpath, "wb");
            if (!newdir) {
                perror("mkdir fopen new dir");
                continue;
            }

            // write "." entry
            {   //set inode number so the dir can refer to itself
                uint32_t dot_ino = (uint32_t)new_inode;
                //create and empty buffer
                char dot_name[32] = {0};
                //writes the "." 
                strncpy(dot_name, ".", 32);
                //writes the inode num for the entry
                fwrite(&dot_ino, sizeof(uint32_t), 1, newdir);
                //writes the fixed 32-byte name field to the directory for the entry
                fwrite(dot_name, 1, 32, newdir);
            }

            // write ".." entry
            {
                uint32_t dotdot_ino = (uint32_t)current_inode;
                char dotdot_name[32] = {0};
                strncpy(dotdot_name, "..", 32);
                fwrite(&dotdot_ino, sizeof(uint32_t), 1, newdir);
                fwrite(dotdot_name, 1, 32, newdir);
            }

            fclose(newdir);

            // append entry to current directory
            curdir = fopen(curpath, "ab");
            if (!curdir) {
                perror("mkdir reopen current dir for append");
                continue;
            }

            char fixed[32] = {0};
            strncpy(fixed, name, 32);

            uint32_t child = (uint32_t)new_inode;
            fwrite(&child, sizeof(uint32_t), 1, curdir);
            fwrite(fixed, 1, 32, curdir);

            fclose(curdir);

            // update inode table in memory
            inode_type[new_inode] = 'd';



        } else if (strcmp(cmd, "touch") == 0) {
            if (n != 2) {
                fprintf(stderr, "Error: touch requires exactly 1 argument\n");
                continue;
            }
            char name[33];
            strncpy(name, arg, 32);
            name[32] = '\0';
            // check if name exists
            char curpath[32];
            snprintf(curpath, sizeof(curpath), "%d", current_inode);

            FILE *curdir = fopen(curpath, "rb");
            if (!curdir) {
                perror("touch fopen current dir");
                continue;
            }

            uint32_t e_inode;
            char e_name[32];
            int exists = 0;

            while (fread(&e_inode, sizeof(uint32_t), 1, curdir) == 1 &&
                fread(e_name, 1, 32, curdir) == 32) {

                char printable[33];
                memcpy(printable, e_name, 32);
                printable[32] = '\0';

                if (strncmp(printable, name, 32) == 0) {
                    exists = 1;
                    break;
                }
            }
            fclose(curdir);

            if (exists) {
                continue;
            }

            // find a free inode
            int new_inode = -1;
            for (int i = 0; i < 1024; i++) {
                if (inode_type[i] == 0) {
                    new_inode = i;
                    break;
                }
            }
            if (new_inode < 0) {
                fprintf(stderr, "touch: no free inodes\n");
                continue;
            }

            // create the new file inode file and write the name into it
            char newpath[32];
            snprintf(newpath, sizeof(newpath), "%d", new_inode);

            FILE *nf = fopen(newpath, "wb");
            if (!nf) {
                perror("touch fopen new file");
                continue;
            }

            fwrite(name, 1, strlen(name), nf);
            fwrite("\n", 1, 1, nf);
            fclose(nf);

            // append entry to current directory
            curdir = fopen(curpath, "ab");
            if (!curdir) {
                perror("touch reopen current dir for append");
                continue;
            }

            char fixed[32] = {0};
            strncpy(fixed, name, 32);

            uint32_t child = (uint32_t)new_inode;
            fwrite(&child, sizeof(uint32_t), 1, curdir);
            fwrite(fixed, 1, 32, curdir);

            fclose(curdir);

            // update inode table in memory
            inode_type[new_inode] = 'f';
        } else {
            fprintf(stderr, "Error: unknown command '%s'\n", cmd);
        }
        

     }
     //overwrite the old version so changes cna be saved
    FILE *out = fopen("inodes_list", "wb");
    if (!out) {
        perror("fopen inodes_list for write");
        return 1; 
    }
    //iterate through every possible inode
    for (uint32_t i = 0; i < 1024; i++) {
        //select only the inodes that are currently in use
        if (inode_type[i] == 'd' || inode_type[i] == 'f') {
            //write the inode number to the file in binary format
            fwrite(&i, sizeof(uint32_t), 1, out);
            //write the inode type 
            fwrite(&inode_type[i], sizeof(char), 1, out);
        }
    }
    fclose(out);

    return 0;
}
