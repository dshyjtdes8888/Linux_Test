// 作业二：

// 编写C程序，完成以下功能：
// 将指定目录下的匹配文件合并成一个大小不超过3MB的文件

// 程序执行的命令形如：
// mkfile  [-p <path>] [-e <ext>]  [-s <min-max>] [-o <file>]
// [-p <path>]： path为指定目录，默认为当前目录
// [-e <ext>]: 匹配文件的扩展名，默认为所有文件
// [-s <min-max>]：源文件大小介于min与max KB字节之间，默认为不限制大小
// [-o <file>]：file为合并后的文件名，默认为 ex1_2022

// 例如：
// mkfile
// 表示在当前目录下的文件合并成文件ex1_2022

// mkfile -s 10-50 -p $home/test -o 111201
// 表示 将目录$home/test中的文件（大小介于10KB~50KB）合并成当前目录下文件111201

// mkfile -e *.c -o test.c
// 表示 将当前目录中扩展名为".c"的文件合并成当前目录下文件test.c

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#define MAX_OUTPUT_SIZE (3 * 1024 * 1024) // 3MB

void mergeFiles(const char *outputFileName, const char *dirName, const char *fileExtension, int minSize, int maxSize)
{
    DIR *dir;
    struct dirent *ent;
    char **allfile = (char **)malloc(MAX_OUTPUT_SIZE * sizeof(char *)); // 用于暂时存储合并文件后的所有字符数据
    int totalSize = 0;                                                  // 用于记录合并文件的总大小防止超过最大限制

    if (!allfile)
    {
        perror("内存分配失败");
        return;
    }

    dir = opendir(dirName);
    if (!dir)
    {
        perror("打开目录失败");
        free(allfile);
        return;
    }

    int i = 0; // allfile数组的索引，用于遍历每个读取到文件的buffer写入数组
    while ((ent = readdir(dir)) != NULL && i < MAX_OUTPUT_SIZE) // 遍历目录依次获取每一个需要合并的文件
    {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/%s", dirName, ent->d_name);

        int fd = open(filename, O_RDONLY);
        if (fd == -1)
        {
            perror("打开文件失败");
            free(allfile);
            return;
        }

        struct stat fileStat;
        if (fstat(fd, &fileStat) == -1)
        {
            perror("获取文件大小失败");
            close(fd);
            free(allfile);
            return;
        }

        if (S_ISREG(fileStat.st_mode)) // 只处理普通文件，忽略可执行文件等
        {
            off_t fileSize = fileStat.st_size;
            if ((fileSize >= minSize) && (fileSize <= maxSize)) // 命令参数之大小限制判断
            {
                if ((fileExtension == NULL || (fileExtension != NULL && strstr(ent->d_name, fileExtension) != NULL))) // 命令参数之后缀判断
                {
                    if (totalSize + fileSize > MAX_OUTPUT_SIZE)
                    {
                        printf("合并文件大小超过最大限制3MB\n");
                        close(fd);
                        free(allfile);
                        return;
                    }

                    char *buffer = (char *)malloc(fileSize);
                    if (!buffer)
                    {
                        perror("分配内存失败");
                        close(fd);
                        free(allfile);
                        return;
                    }

                    ssize_t bytesRead = read(fd, buffer, fileSize);
                    if (bytesRead == -1)
                    {
                        perror("获取文件内容失败");
                        free(buffer);
                        close(fd);
                        free(allfile);
                        return;
                    }
                    close(fd);

                    allfile[i] = buffer;
                    totalSize += bytesRead;
                    i++;
                }
                else
                {
                    close(fd); // 如果不是指定后缀的文件，关闭文件
                }
            }
            else
            {
                close(fd); // 如果文件大小不在指定范围内，关闭文件
            }
        }
        else
        {
            close(fd); // 如果不是普通文件，关闭文件
        }
    }
    closedir(dir);

    if (totalSize > MAX_OUTPUT_SIZE)
    {
        printf("合并文件大小超过最大限制3MB\n");
        free(allfile);
        return;
    }

    // 打开需要输出合并内容的文件
    int outputFd = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (outputFd == -1)
    {
        perror("打开输出文件失败");
        free(allfile);
        return;
    }

    // 将所有内容写入输出文件
    for (int j = 0; j < i; j++)
    {
        ssize_t bytesWritten = write(outputFd, allfile[j], strlen(allfile[j]));
        if (bytesWritten == -1)
        {
            perror("写入输出文件失败");
            close(outputFd);
            free(allfile);
            return;
        }
    }

    close(outputFd);

    // 释放动态分配的内存
    for (int j = 0; j < i; j++)
    {
        free(allfile[j]);
    }

    free(allfile);
    printf("合并文件完成\n");
}


int main(int argc, char *argv[])
{
    //给出参数默认值
    const char *dirName = ".";
    const char *fileExtension = NULL;
    int minSize = 0;
    int maxSize = MAX_OUTPUT_SIZE;
    char *outputFileName = "ex1_2023";

    int opt;
    // 使用getopt用于解析命令行参数
    while ((opt = getopt(argc, argv, "p:e:s:o:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            dirName = optarg;
            break;
        case 'e':
            fileExtension = optarg;
            break;
        case 's':
            if (sscanf(optarg, "%d-%d", &minSize, &maxSize) != 2)
            {
                printf("指令参数格式错误应为min-max\n");
                return 1;
            }
            minSize = minSize * 1024;
            maxSize = maxSize * 1024;
            break;
        case 'o':
            outputFileName = optarg;
            break;
        default:
            return 1;
        }
    }
    mergeFiles(outputFileName, dirName, fileExtension, minSize, maxSize);

    return 0;
}