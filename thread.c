//作业三：使用多线程编程技术编写程序实现作业二的功能。
//基本要求:
//1. 主线程收集符合条件的文件信息；
//2. 其它线程实现文件的归并，每个线程负责一个或多个文件内容的归并；
//3. 若符合条件的文件有三个及以上，则至少需要三个线程，即主线程、两个及以上的其它对等线程。

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>

#define MAX_OUTPUT_SIZE (3 * 1024 * 1024) // 3MB

pthread_mutex_t offset_mutex; //创建锁用于防止next_offest读冲突
int next_offset = 0;  //用于存储下一个线程需要写入的位置

//用于创建线程执行函数时传参
struct ThreadArgs
{
    const char *outputFileName;
    char *fileContent;
    int fileSize;
    off_t fileOffset;
};

//线程调用的函数，用于从制定位置写入每个线程负责的文件内容
void *writetofile(void *arg)
{
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    const char *outputFileName = args->outputFileName;
    char *fileContent = args->fileContent;
    int fileSize = args->fileSize;
    off_t fileOffset = args->fileOffset;

    int outputFd = open(outputFileName, O_WRONLY | O_CREAT, 0666);
    if (outputFd == -1)
    {
        perror("打开输出文件失败");
        free(fileContent);
        return NULL;
    }

    pthread_mutex_lock(&offset_mutex); 
    lseek(outputFd, fileOffset, SEEK_SET); //移动文件指针到该线程需要负责的位置
    ssize_t bytesWritten = write(outputFd, fileContent, fileSize);
    if (bytesWritten == -1)
    {
        perror("写入输出文件失败");
    }
    pthread_mutex_unlock(&offset_mutex); 

    close(outputFd);
    free(fileContent);

    printf("合并文件完成\n");

    return NULL;
}

//用于遍历目录获取文件信息，并且为每个符合条件的文件创建线程进行写入
void mergeFiles(const char *outputFileName, const char *dirName, const char *fileExtension, int minSize, int maxSize)
{
    DIR *dir;
    struct dirent *ent;

    dir = opendir(dirName);
    if (!dir)
    {
        perror("打开目录失败");
        return;
    }

    char filename[1024];
    while ((ent = readdir(dir)) != NULL)
    {
        snprintf(filename, sizeof(filename), "%s/%s", dirName, ent->d_name);
        int fd = open(filename, O_RDONLY);

        if (fd == -1)
        {
            perror("打开文件失败");
            return;
        }

        struct stat fileStat;
        if (fstat(fd, &fileStat) == -1)
        {
            perror("获取文件大小失败");
            close(fd);
            return;
        }

        if (S_ISREG(fileStat.st_mode) && (fileStat.st_mode & S_IXUSR) == 0) //过滤掉可执行文件，可执行文件写入会导致乱码
        {
            off_t fileSize = fileStat.st_size;
            if ((fileSize >= minSize) && (fileSize <= maxSize))
            {
                if (fileExtension == NULL || (fileExtension != NULL && strstr(ent->d_name, fileExtension) != NULL))
                {
                    if (next_offset + fileSize > MAX_OUTPUT_SIZE)
                    {
                        printf("合并文件大小超过最大限制3MB\n");
                        close(fd);
                        return;
                    }

                    char *buffer = (char *)malloc(fileSize);
                    if (!buffer)
                    {
                        perror("分配内存失败");
                        close(fd);
                        return;
                    }

                    ssize_t bytesRead = read(fd, buffer, fileSize);
                    if (bytesRead == -1)
                    {
                        perror("获取文件内容失败");
                        free(buffer);
                        close(fd);
                        return;
                    }

                    struct ThreadArgs *threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
                    threadArgs->outputFileName = outputFileName;
                    threadArgs->fileContent = buffer;
                    threadArgs->fileSize = bytesRead;
                    threadArgs->fileOffset = next_offset;

                    pthread_t thread;
                    if (pthread_create(&thread, NULL, writetofile, threadArgs) != 0)
                    {
                        perror("创建线程失败");
                    }

                    pthread_join(thread, NULL);

                    next_offset += fileSize;

                    close(fd);
                }
                else
                {
                    close(fd);
                }
            }
            else
            {
                close(fd);
            }
        }
        else
        {
            close(fd);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[])
{
    const char *dirName = ".";
    const char *fileExtension = NULL;
    int minSize = 0;
    int maxSize = MAX_OUTPUT_SIZE;
    char *outputFileName = "ex1_2023.txt";

    int opt;
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

    pthread_mutex_init(&offset_mutex, NULL); //初始化锁

    mergeFiles(outputFileName, dirName, fileExtension, minSize, maxSize);

    pthread_mutex_destroy(&offset_mutex);  //锁销毁

    return 0;
}