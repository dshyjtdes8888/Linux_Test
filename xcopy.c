#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>

#define MAX_PATH_LENGTH 1024
#define MSG_SIZE 8192

// 消息队列结构
struct msg_buffer
{
    long mtype;
    char mtext[MSG_SIZE];
};

int mkdirs(char *path)
{
    char str[512];
    strncpy(str, path, 512);
    int len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        if (str[i] == '/') // 逐级检查各级目录
        {
            str[i] = '\0';
            if (access(str, 0) != 0) // 如果访问该目录返回不成功
            {
                mkdir(str, 0777); // 则创建该目录, 这里是逐级创建的.
            }
            str[i] = '/';
        }
    }
    if (len > 0 && access(str, 0) != 0) // 检查最后一级目录
    {
        mkdir(str, 0777); // 若不可访问,则创建该目录.
    }
    struct stat s;
    stat(path, &s);
    if (S_ISDIR(s.st_mode))
        return 0;
    return 1;
}

int main(int argc, char *argv[])
{
    int msgid_B, msgid_C;         // 分别为进程B和进程C创建的消息队列
    key_t key_B = ftok(".", 'B'); // 创建进程B的消息队列键
    key_t key_C = ftok(".", 'C'); // 创建进程C的消息队列键

    if (key_B == -1 || key_C == -1)
    {
        perror("ftok");
        exit(1);
    }

    msgid_B = msgget(key_B, 0666 | IPC_CREAT); // 创建进程B的消息队列
    msgid_C = msgget(key_C, 0666 | IPC_CREAT); // 创建进程C的消息队列

    if (msgid_B == -1 || msgid_C == -1)
    {
        perror("msgget");
        exit(1);
    }

    pid_t pidB, pidC;
    char *srcpath = ".";
    char *dstpath = NULL;
    struct msg_buffer msg_B, msg_C;
    msg_B.mtype = 1;
    msg_C.mtype = 1;

    // 解析命令行参数
    if (argc < 3)
    {
        printf("Usage: xcopy [-i <srcpath>] -o <dstpath>\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-i") == 0)
        {
            if (i + 1 < argc)
            {
                srcpath = argv[i + 1];
                i++;
            }
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
            {
                dstpath = argv[i + 1];
                i++;
            }
        }
        else
        {
            printf("Usage: xcopy [-i <srcpath>] -o <dstpath>\n");
            exit(1);
        }
    }

    // 检查源目录是否存在
    struct stat st;
    if (stat(srcpath, &st) != 0)
    {
        printf("源目录不存在，请重新输入\n");
        // 删除消息队列
        msgctl(msgid_B, IPC_RMID, NULL);
        msgctl(msgid_C, IPC_RMID, NULL);
        return 0;
    }

    //目的目录检查
    if (dstpath == NULL)
    {
        printf("目的目录不能为空，请重新输入\n");
        // 删除消息队列
        msgctl(msgid_B, IPC_RMID, NULL);
        msgctl(msgid_C, IPC_RMID, NULL);
        return 0;
    }
    if (stat(dstpath, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            printf("目的目录存在\n");
        }
        else
        {
            printf("指定路径存在，但不是一个目录\n");
        }
    }
    else
    {
        printf("目的目录不存在\n");
        // 目录不存在，逐级创建
        int ret = mkdirs(dstpath);
        if (ret == 0)
        {
            printf("创建目的目录成功\n");
        }
        else
        {
            printf("创建目的目录失败\n");
        }
    }

    // 创建进程B
    pidB = fork();
    if (pidB == 0)
    {
        // 子进程B
        DIR *dir;
        struct dirent *entry;

        dir = opendir(srcpath);
        if (dir == NULL)
        {
            perror("打开源目录失败");
            exit(1);
        }

        char filename[1024];
        int filecount = 0; // 初始化文件数量为0
        while ((entry = readdir(dir)) != NULL)
        {
            snprintf(filename, sizeof(filename), "%s/%s", srcpath, entry->d_name);
            int fd = open(filename, O_RDONLY);

            if (fd == -1)
            {
                perror("打开文件失败");
                exit(1);
            }

            struct stat fileStat;
            if (fstat(fd, &fileStat) == -1)
            {
                perror("获取文件大小失败");
                close(fd);
                exit(1);
            }

            if (S_ISREG(fileStat.st_mode) && (fileStat.st_mode & S_IXUSR) == 0)
            {
                off_t fileSize = fileStat.st_size;

                if (fileSize + 1 >= 16384)
                {
                    perror("单个文件过大，无法复制");
                    close(fd);
                    exit(1);
                }

                char *buffer = (char *)malloc(fileSize + 1);
                // char buffer[MSG_SIZE];
                if (!buffer)
                {
                    perror("分配内存失败");
                    close(fd);
                    exit(1);
                }

                ssize_t bytesRead = read(fd, buffer, fileSize);
                if (bytesRead == -1)
                {
                    perror("获取文件内容失败");
                    free(buffer);
                    close(fd);
                    exit(1);
                }

                // 将文件名和文件内容发送到进程C
                sprintf(msg_B.mtext, "%s", entry->d_name);
                msgsnd(msgid_C, &msg_B, sizeof(msg_B.mtext), 0);

                char buffer_part[8192];
                if (fileSize + 1 >= 8192)
                {
                    sprintf(msg_B.mtext, "%s", buffer);
                    msgsnd(msgid_C, &msg_B, sizeof(msg_B.mtext), 0);
                    for (int i = 8192; i < fileSize + 1; i++)
                    {
                        buffer_part[i - 8192] = buffer[i];
                    }
                    buffer_part[fileSize - 8192] = '\0';
                    filecount++;
                    sprintf(msg_B.mtext, "%s", buffer_part);
                    msgsnd(msgid_C, &msg_B, sizeof(msg_B.mtext), 0);
                }
                else
                {
                    // 添加空终止符
                    buffer[fileSize] = '\0';
                    filecount++;

                    sprintf(msg_B.mtext, "%s", buffer);
                    msgsnd(msgid_C, &msg_B, sizeof(msg_B.mtext), 0);
                }

                free(buffer);
            }
            close(fd);
        }

        strcpy(msg_B.mtext, "end");
        msgsnd(msgid_C, &msg_B, strlen(msg_B.mtext) + 1, 0);

        while (1)
        {
            struct msg_buffer msg_B;
            msgrcv(msgid_B, &msg_B, sizeof(msg_B.mtext), 1, 0);
            if (strncmp(msg_B.mtext, "Success", 7) == 0)
            {
                printf("Process C finished\n");
                break;
            }
        }

        // 将文件数量发送到进程A
        sprintf(msg_B.mtext, "%d", filecount);
        msgsnd(msgid_B, &msg_B, sizeof(msg_B.mtext), 0);
        printf("Process B finished\n");

        exit(0);
    }
    else if (pidB > 0)
    {
        // 父进程A
        pidC = fork();
        if (pidC == 0)
        {
            // 子进程C
            int totalFileSize = 0;

            while (1)
            {
                struct msg_buffer msg_C;
                msgrcv(msgid_C, &msg_C, sizeof(msg_C.mtext), 1, 0);
                if (strncmp(msg_C.mtext, "end", 3) == 0)
                {
                    break;
                }
                char filename[1024];
                char filecontent[16384];

                // 接收文件名
                strcpy(filename, msg_C.mtext);

                // 接收文件内容
                msgrcv(msgid_C, &msg_C, sizeof(msg_C.mtext), 1, 0);
                strcpy(filecontent, msg_C.mtext);
                while (strlen(filecontent) == 8192)
                {
                    msgrcv(msgid_C, &msg_C, sizeof(msg_C.mtext), 1, 0);
                    strcat(filecontent, msg_C.mtext);
                }

                // 将文件内容写入目标文件
                totalFileSize += strlen(filecontent);
                char dstfilename[MAX_PATH_LENGTH];
                snprintf(dstfilename, sizeof(dstfilename), "%s/%s", dstpath, filename);
                int fd = open(dstfilename, O_WRONLY | O_CREAT, 0666);
                if (fd == -1)
                {
                    perror("打开目标文件失败");
                    exit(1);
                }
                write(fd, filecontent, strlen(filecontent));
                close(fd);
            }

            // 成功时发送成功消息给进程B
            strcpy(msg_C.mtext, "Success");
            msgsnd(msgid_B, &msg_C, strlen(msg_C.mtext) + 1, 0);

            // 发送文件总大小到进程A
            sprintf(msg_C.mtext, "%d", totalFileSize);
            msgsnd(msgid_C, &msg_C, sizeof(msg_C.mtext), 0);

            exit(0);
        }
        else if (pidC > 0)
        {
            // 父进程A
            int statusB, statusC;
            waitpid(pidB, &statusB, 0);
            waitpid(pidC, &statusC, 0);

            if (statusB == 0 && statusC == 0)
            {
                // 接收文件总大小
                msgrcv(msgid_C, &msg_C, sizeof(msg_C.mtext), 1, 0);
                int totalFileSize = atoi(msg_C.mtext);
                printf("总文件大小: %d bytes\n", totalFileSize);

                // 接收文件数量
                msgrcv(msgid_B, &msg_B, sizeof(msg_B.mtext), 1, 0);
                int totalFileCount = atoi(msg_B.mtext);
                printf("总文件数量: %d\n", totalFileCount);
            }
            else
            {
                printf("子进程异常退出\n");
                // 删除消息队列
                msgctl(msgid_B, IPC_RMID, NULL);
                msgctl(msgid_C, IPC_RMID, NULL);
                return 0;
            }
        }
    }
    else
    {
        perror("进程创建出错");
        exit(1);
    }

    // 删除消息队列
    msgctl(msgid_B, IPC_RMID, NULL);
    msgctl(msgid_C, IPC_RMID, NULL);

    return 0;
}
