#include <gd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "image-lib.h"
#include <string.h>
#include <unistd.h>

#define OLD_IMAGE_DIR "./old_photo_PAR_B/"
#define MAX_WORDS 1000
#define MAX_LENGTH 100

typedef struct ThreadArgs {
    int threadID;
    char *files[MAX_WORDS];
    char *filesout[MAX_WORDS];
    int numImagens;
    int numThreads;
    int *pipe_fd;
    int numImagensperThread;
    double executionTime;
    double executionTimeImg[1000];
    char *IDimageName[100];
} ThreadArgs;

double calculate_execution_time(struct timespec start, struct timespec end) {
    //devolve o tempo de execução da thread
    return (end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1.0e9;
}

char* deleteBeforeLastSlash(const char *str) {
    char *result = NULL;
    char *lastSlash = strrchr(str, '/');
    if (lastSlash != NULL) {
        size_t index = lastSlash - str + 1;
        result = (char *)malloc(strlen(str) - index + 1);
        if (result != NULL) {
            strcpy(result, &str[index]);
        }
    } else {
        result = (char *)malloc(strlen(str) + 1);
        if (result != NULL) {
            strcpy(result, str);
        }
    }
    return result;
}

void lerNomeImagens(char *nomeFicheiro, char *files[], int *numImagens) {
    FILE *file = fopen(nomeFicheiro, "r");
    
    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    char word[MAX_LENGTH];
    int count = 0;
    while (fgets(word, MAX_LENGTH, file) != NULL && count < MAX_WORDS) {//guarda os nomes das imagens no vetor files
        word[strcspn(word, "\n")] = '\0';
        files[count] = strdup(word); 
        count++;
    }

    fclose(file);
    *numImagens = count;//passa o número de imagens para a estrutura
}

void *resize_photos(void *arg) {
    struct timespec start_time_thread, end_time_thread;
    
    
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    clock_gettime(CLOCK_MONOTONIC, &start_time_thread);

    int *pipe_fd=threadArg->pipe_fd;
    
    
    // Verificar se há menos imagens do que threads
    if (threadArg->numThreads > threadArg->numImagens && threadArg->threadID >= threadArg->numImagens) {
    pthread_exit(NULL); // Encerra a thread extra
    }

    char out_file_name[100];

    char filePipe[100];
    gdImagePtr in_img;
    gdImagePtr out_smoothed_img;
    gdImagePtr out_contrast_img;
    gdImagePtr out_textured_img;
    gdImagePtr out_sepia_img;
    gdImagePtr in_texture_img =  read_png_file("./paper-texture.png");
    int Nimagens=0;
    
    //Aplica os filtros à imagem
    while(read(pipe_fd[0], &filePipe, sizeof(filePipe))>0) {
    struct timespec start_time_image, end_time_image;
    clock_gettime(CLOCK_MONOTONIC, &start_time_image); 


        
        char *filePipeOut = deleteBeforeLastSlash(filePipe);

        
        if (filePipeOut == NULL) {
            fprintf(stderr, "Memory allocation failed for filePipeOut\n");
            continue; // Skip processing if memory allocation fails
        }
        
        printf("Thread_%d processing image %s\n", threadArg->threadID, filePipe);
        sprintf(out_file_name, "%s%s", OLD_IMAGE_DIR, filePipeOut);

        if( access( out_file_name, F_OK ) != -1){
            continue;
        }
        
        
        in_img = read_jpeg_file(filePipe);
        
        if (in_img == NULL) {
            fprintf(stderr, "Impossible to read %s image\n", filePipe);
            continue;
        }
        
        out_contrast_img = contrast_image(in_img);
        out_smoothed_img = smooth_image(out_contrast_img);
        out_textured_img = texture_image(out_smoothed_img, in_texture_img);
        out_sepia_img = sepia_image(out_textured_img);

        
        if(write_jpeg_file(out_sepia_img, out_file_name) == 0){
            fprintf(stderr, "Impossible to write %s image\n", out_file_name);
        }
        
        gdImageDestroy(out_smoothed_img);
        gdImageDestroy(out_sepia_img);
        gdImageDestroy(out_contrast_img);
        gdImageDestroy(in_img);
        
        
        threadArg->IDimageName[Nimagens] = filePipeOut;
        
        
        clock_gettime(CLOCK_MONOTONIC, &end_time_image);
        
        threadArg->executionTimeImg[Nimagens] = calculate_execution_time(start_time_image, end_time_image);

        Nimagens++;
    }
    threadArg->numImagensperThread=Nimagens;
    clock_gettime(CLOCK_MONOTONIC, &end_time_thread);
    

    threadArg->executionTime = calculate_execution_time(start_time_thread, end_time_thread);
    pthread_exit(NULL);
}

void cria_threads(ThreadArgs *threadArg) {
    pthread_t threads[threadArg->numThreads];
    int *pipe_fd=threadArg->pipe_fd;
    for (int d = 0; d < threadArg->numThreads; d++) {
        threadArg[d].threadID = d;
        pthread_create(&threads[d], NULL, resize_photos, (void *)&threadArg[d]);
    }

    for (int i = 0; i < threadArg->numImagens; i++) {
        //int targetThread = i % threadArg->numThreads; // Escolhe a thread que vai receber a imagem atual
        write(pipe_fd[1], threadArg->files[i], MAX_LENGTH * sizeof(char));
    }

    for (int i = 0; i < threadArg->numThreads; i++) {
        close(pipe_fd[1]); // Fecha a escrita no pipe
    }

    for (int d = 0; d < threadArg->numThreads; d++) {
        pthread_join(threads[d], NULL);
    }

    
    //free(pipe_fd);
}

int main(int argc, char *argv[]) {
    //Inicialização das estruturas de tempo
    struct timespec start_time_total, end_time_total;
    //struct timespec start_time_seq, end_time_seq;
    //struct timespec start_time_par, end_time_par;

    clock_gettime(CLOCK_MONOTONIC, &start_time_total);
    //clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

    
    char *files[MAX_WORDS];//Declaração do vetor onde vai ser guardado os títulos das imagens
    int numImagens;
    char in_file_name[100];//Declaração da string onde vai ser guardado o caminho para ler a lista de imagens
    int pipe_fd[2];

    if (argc > 3) {//Verificação dos argumentos do terminal
        printf("Foram passados argumentos em demasia.\n");
        exit(EXIT_FAILURE);
    }
    if (argc < 3) {
        printf("Não foram passados argumentos suficientes.\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Argumentos passados:\n");
        // Imprime os vários argumentos com o argv
        for (int i = 0; i < argc; i++) {
            printf("Argumento %d: %s\n", i, argv[i]);
        }
    }

    if (pipe(pipe_fd) != 0) {
            printf("Error creating the pipe\n");
            exit(-1);
        }


    sprintf(in_file_name, "%s/image-list.txt", argv[1]);//caminho para ler a lista de imagens


    lerNomeImagens(in_file_name, files, &numImagens);//função para ler a lista de imagens image-list.txt

    int numThreads=atoi(argv[2]);;
    printf("Number of threads to create: %d\n\n",numThreads);

    ThreadArgs *threadArg = malloc(sizeof(ThreadArgs) * numThreads);
    if (threadArg == NULL) {
        fprintf(stderr, "Memory allocation failed for threadArg\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numThreads; i++) {
        threadArg[i].executionTime = 0.0; // Inicializa os tempos de execução
    }
    char **completeFilePaths = malloc(numImagens * sizeof(char *));
    if (completeFilePaths == NULL) {
        fprintf(stderr, "Memory allocation failed for completeFilePaths\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numThreads; i++) {
        threadArg[i].numThreads = numThreads;
        threadArg[i].numImagens = numImagens;
        threadArg[i].pipe_fd=pipe_fd;

        for (int j = 0; j < numImagens; j++) {
            completeFilePaths[j] = malloc(MAX_LENGTH * sizeof(char));
            if (completeFilePaths[j] == NULL) {
                fprintf(stderr, "Memory allocation failed for completeFilePaths[%d]\n", j);
                exit(EXIT_FAILURE);
            }
            
            // Junta o diretório fornecido pelo terminal com o nome da imagem
            sprintf(completeFilePaths[j], "%s/%s", argv[1], files[j]);

            // Atribuir o novo caminho completo ao vetor files e filesout(imagens de saída) na estrutura
            threadArg[i].files[j] = completeFilePaths[j];
            threadArg[i].filesout[j] = files[j];
            
        }
    }


    /* creation of output directories */
    if (create_directory(OLD_IMAGE_DIR) == 0){
        fprintf(stderr, "Impossible to create %s directory\n", OLD_IMAGE_DIR);
        exit(EXIT_FAILURE);
    }

    
    //clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
    //clock_gettime(CLOCK_MONOTONIC, &start_time_par);

    cria_threads(threadArg);//Função que cria as várias threads



    //clock_gettime(CLOCK_MONOTONIC, &end_time_par);
    clock_gettime(CLOCK_MONOTONIC, &end_time_total);

    /*struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);
    struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);*/
    struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);
    /*printf("\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
    printf("\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);*/


    
    //Criação do ficheiro timings.txt
    char nomeFicheirorelatorio[100];
    sprintf(nomeFicheirorelatorio, "%stiming_%d.txt", OLD_IMAGE_DIR,numThreads);

    FILE *relatorio = fopen(nomeFicheirorelatorio, "w");
    if (relatorio == NULL) {
        perror("Erro ao criar o arquivo");
        exit(EXIT_FAILURE);
    }else{
    

    for (int i = 0; i < numThreads; i++) {
    for (int j = 0; j < threadArg[i].numImagensperThread; j++) {
            fprintf(relatorio, "%s\t%.2f\n", threadArg[i].IDimageName[j], threadArg[i].executionTimeImg[j]);
        }
    }


    for (int i = 0; i < numThreads; i++) {
        fprintf(relatorio, "Thread %d\t%d\t%.9f\n",threadArg[i].threadID,threadArg[i].numImagensperThread ,threadArg[i].executionTime);
        
    }
    }

    fprintf(relatorio,"total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);

    fclose(relatorio);


    printf("\n\n");
    for (int i = 0; i < numThreads; i++) {
        for (int j = 0; j < threadArg[i].numImagensperThread; j++) {
                printf("%s\t%.2f\n", threadArg[i].IDimageName[j], threadArg[i].executionTimeImg[j]);
            }
    }
    for (int i = 0; i < numThreads; i++)//impressão do tempo de cada thread
    {
        printf("thread_%d \t %d \t %.9f\n", threadArg[i].threadID,threadArg[i].numImagensperThread ,threadArg[i].executionTime);
    }
   
    printf("total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);
    
    // Free dynamically allocated memory
    free(threadArg);
    for (int i = 0; i < numImagens; i++) {
        free(files[i]);
    }
    
    return 0;
}

