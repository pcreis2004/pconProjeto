#include <gd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "image-lib.h"
#include <string.h>
#include <unistd.h>

#define OLD_IMAGE_DIR "./old_photo_PIPELINE/"
#define MAX_WORDS 1000
#define MAX_LENGTH 100

typedef struct ThreadArgs {
    int threadID;
    char *files[MAX_WORDS];
    char *filesout[MAX_WORDS];
    int numImagens;
    
    int *pipe_fd;
    int *pipe_fd_contrast_to_smooth;

    int *pipe_fd_textured_to_sepia;

    int *pipe_fd_smooth_to_textured;

    //int *pipe_fd_sepia;

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

void *contrast(void *arg){
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    printf("Conseguiste entrar aqui dentro campeão todo contrastado\n");
    int *pipe_fd=threadArg->pipe_fd;
    int *pipe_fd_contrast_to_smooth=threadArg->pipe_fd_contrast_to_smooth;

    
    // Verificar se há menos imagens do que threads
    
    

    char filePipe[100];
    gdImagePtr in_img;
    gdImagePtr out_contrast_img;
    while(read(pipe_fd[0], &filePipe, sizeof(filePipe))>0){
        printf("Conseguiste ler\n");
	printf("Foi lido da Pipe a seguinte string --> %s\n",filePipe);
        if (filePipe == NULL) {
            fprintf(stderr, "Memory allocation failed for filePipeOut\n");
            //continue; // Skip processing if memory allocation fails
            exit(EXIT_FAILURE);
        }

        in_img = read_jpeg_file(filePipe);
                printf("Conseguiste ler a imagem\n");

        if (in_img == NULL) {
            fprintf(stderr, "Impossible to read %s image\n", filePipe);
            //continue;
            exit(EXIT_FAILURE);
        }

        out_contrast_img = contrast_image(in_img);
        write(pipe_fd_contrast_to_smooth[1],in_img,sizeof(in_img));
        threadArg->pipe_fd_contrast_to_smooth=pipe_fd_contrast_to_smooth;
        gdImageDestroy(in_img);
        gdImageDestroy(out_contrast_img);


    }
    pthread_exit(NULL);
}

void *smooth(void *arg){
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    printf("Conseguiste entrar aqui dentro campeão todo smoodado\n");

    int *pipe_fd_contrast_smooth=threadArg->pipe_fd_contrast_to_smooth;
    int *pipe_fd_smooth_to_textured=threadArg->pipe_fd_smooth_to_textured;

    gdImagePtr out_contrast_img;
    gdImagePtr out_smoothed_img;
    
    while(read(pipe_fd_contrast_smooth[0],&out_contrast_img,sizeof(out_contrast_img))>0){
        out_smoothed_img = smooth_image(out_contrast_img);
        write(pipe_fd_smooth_to_textured[1],out_smoothed_img,sizeof(out_smoothed_img));
        threadArg->pipe_fd_smooth_to_textured=pipe_fd_smooth_to_textured;
        gdImageDestroy(out_contrast_img);
        gdImageDestroy(out_smoothed_img);


    }
    pthread_exit(NULL);
}

void *texture(void *arg){
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    printf("Conseguiste entrar aqui dentro campeão todo texturado\n");
    int *pipe_fd_smooth_to_textured=threadArg->pipe_fd_smooth_to_textured;
    int *pipe_fd_textured_to_sepia=threadArg->pipe_fd_textured_to_sepia;

    gdImagePtr out_smoothed_img;
    gdImagePtr in_textured_img= read_jpeg_file("./paper-texture.png");
    gdImagePtr out_textured_img;


    
    while(read(pipe_fd_smooth_to_textured[0],&out_smoothed_img,sizeof(out_smoothed_img))>0){
        out_textured_img = texture_image(out_smoothed_img, in_textured_img);
        write(pipe_fd_textured_to_sepia[1],out_textured_img,sizeof(out_textured_img));
        threadArg->pipe_fd_textured_to_sepia=pipe_fd_textured_to_sepia;
        gdImageDestroy(out_smoothed_img);
        gdImageDestroy(in_textured_img);
        gdImageDestroy(out_textured_img);

    }
    pthread_exit(NULL);
}

void *sepia(void *arg){
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    printf("Conseguiste entrar aqui dentro campeão todo sepiado\n");
    int *pipe_fd_textured=threadArg->pipe_fd_textured_to_sepia;
    char *filePipe;
    gdImagePtr out_textured_img;
    gdImagePtr out_sepia_img;
    char **filePipeOut = threadArg->filesout;
    char out_file_name[100];
    while(read(pipe_fd_textured[0],&out_textured_img,sizeof(out_textured_img))>0){
        out_sepia_img = sepia_image(out_textured_img);


        char *filePipeOut = deleteBeforeLastSlash(filePipe);
        sprintf(out_file_name, "%s%s", OLD_IMAGE_DIR, filePipeOut); 
        if(write_jpeg_file(out_sepia_img, out_file_name) == 0){
            fprintf(stderr, "Impossible to write %s image\n", out_file_name);
        }
    gdImageDestroy(out_textured_img);
    gdImageDestroy(out_sepia_img);
    }
    pthread_exit(NULL);
    
}

void cria_pipeline(ThreadArgs *threadArg) {

    pthread_t thread_contrast;
    pthread_t thread_smooth;
    pthread_t thread_textured;
    pthread_t thread_sepia;
    
    int pipe_fd_contrast_to_smooth[2];
    int pipe_fd_smooth_to_textured[2];
    int pipe_fd_textured_to_sepia[2];
    if (pipe(pipe_fd_contrast_to_smooth) != 0) {
            printf("Error creating the pipe\n");
            exit(-1);
        }
    if (pipe(pipe_fd_smooth_to_textured) != 0) {
            printf("Error creating the pipe\n");
            exit(-1);
        }
    if (pipe(pipe_fd_textured_to_sepia) != 0) {
            printf("Error creating the pipe\n");
            exit(-1);
        }

    
    int *pipe_fd=threadArg->pipe_fd;
    printf("Conseguiste sacar o pipe cá para dentro e declaraste as 4 threads\n");
        pthread_create(&thread_contrast, NULL, contrast, (void *)&threadArg[0]);
        pthread_create(&thread_smooth, NULL, smooth, (void *)&threadArg[0]);
        pthread_create(&thread_textured, NULL, texture, (void *)&threadArg[0]);
        pthread_create(&thread_smooth, NULL, sepia, (void *)&threadArg[0]);
    printf("Conseguiste criar as threads\n");
    for (int i = 0; i < threadArg->numImagens; i++) {
        //int targetThread = i % threadArg->numThreads; // Escolhe a thread que vai receber a imagem atual
        write(pipe_fd[1], threadArg->files[i], MAX_LENGTH * sizeof(char));
    }
    printf("Conseguiste escrever os ficheiros na pipe");
    
        close(pipe_fd[1]); // Fecha a escrita no pipe
    

    pthread_join(thread_contrast, NULL);
    pthread_join(thread_smooth, NULL);
    pthread_join(thread_textured, NULL);
    pthread_join(thread_sepia, NULL);


    
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

    if (argc > 2) {//Verificação dos argumentos do terminal
        printf("Foram passados argumentos em demasia.\n");
        exit(EXIT_FAILURE);
    }
    if (argc < 2) {
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
    printf("Ficheiro lido com sucesso\n");
    

    ThreadArgs *threadArg = malloc(sizeof(ThreadArgs));
    if (threadArg == NULL) {
        fprintf(stderr, "Memory allocation failed for threadArg\n");
        exit(EXIT_FAILURE);
    }
    
    threadArg->executionTime = 0.0; // Inicializa os tempos de execução
    
    char **completeFilePaths = malloc(numImagens * sizeof(char *));
    if (completeFilePaths == NULL) {
        fprintf(stderr, "Memory allocation failed for completeFilePaths\n");
        exit(EXIT_FAILURE);
    }

    //for (int i = 0; i < numThreads; i++) {
        //threadArg[i].numThreads = numThreads;
        threadArg[0].numImagens = numImagens;
        threadArg[0].pipe_fd=pipe_fd;

        for (int j = 0; j < numImagens; j++) {
            completeFilePaths[j] = malloc(MAX_LENGTH * sizeof(char));
            if (completeFilePaths[j] == NULL) {
                fprintf(stderr, "Memory allocation failed for completeFilePaths[%d]\n", j);
                exit(EXIT_FAILURE);
            }
            
            // Junta o diretório fornecido pelo terminal com o nome da imagem
            sprintf(completeFilePaths[j], "%s/%s", argv[1], files[j]);

            // Atribuir o novo caminho completo ao vetor files e filesout(imagens de saída) na estrutura
            threadArg[0].files[j] = completeFilePaths[j];
            threadArg[0].filesout[j] = files[j];
            
        }
    


    /* creation of output directories */
    if (create_directory(OLD_IMAGE_DIR) == 0){
        fprintf(stderr, "Impossible to create %s directory\n", OLD_IMAGE_DIR);
        exit(EXIT_FAILURE);
    }

    
    //clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
    //clock_gettime(CLOCK_MONOTONIC, &start_time_par);
    printf("O problema está dentro da pipeline\n");
    cria_pipeline(threadArg);//Função que cria as várias threads



    //clock_gettime(CLOCK_MONOTONIC, &end_time_par);
    clock_gettime(CLOCK_MONOTONIC, &end_time_total);

    /*struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);
    struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);*/
    struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);
    /*printf("\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
    printf("\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);*/


    
    //Criação do ficheiro timings.txt
    char nomeFicheirorelatorio[100];
    sprintf(nomeFicheirorelatorio, "%stimings.txt", OLD_IMAGE_DIR);

    FILE *relatorio = fopen(nomeFicheirorelatorio, "w");
    if (relatorio == NULL) {
        perror("Erro ao criar o arquivo");
        exit(EXIT_FAILURE);
    }else{
    

    /*for (int i = 0; i < numThreads; i++) {
    for (int j = 0; j < threadArg[i].numImagensperThread; j++) {
            fprintf(relatorio, "%s\t%.2f\n", threadArg[i].IDimageName[j], threadArg[i].executionTimeImg[j]);
        }
    }


    for (int i = 0; i < numThreads; i++) {
        fprintf(relatorio, "Thread %d\t%d\t%.9f\n",threadArg[i].threadID,threadArg[i].numImagensperThread ,threadArg[i].executionTime);
        
    }*/
    }

    fprintf(relatorio,"total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);

    fclose(relatorio);


    printf("\n\n");/*
    for (int i = 0; i < numThreads; i++) {
        for (int j = 0; j < threadArg[i].numImagensperThread; j++) {
                printf("%s\t%.2f\n", threadArg[i].IDimageName[j], threadArg[i].executionTimeImg[j]);
            }
    }
    for (int i = 0; i < numThreads; i++)//impressão do tempo de cada thread
    {
        printf("thread_%d \t %d \t %.9f\n", threadArg[i].threadID,threadArg[i].numImagensperThread ,threadArg[i].executionTime);
    }
   */
    printf("total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);
    
    // Free dynamically allocated memory
    free(threadArg);
    for (int i = 0; i < numImagens; i++) {
        free(files[i]);
    }
    
    return 0;
}

