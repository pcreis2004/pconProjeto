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
    double StartTime;
    double executionTimeStart[100];
    double executionTimeImg[100];
    char *IDimageName[100];
    struct timespec start_time_total;
    

} ThreadArgs;

//Fazer estrutura para passar imagens entre estágios;
typedef struct PipeArgs
{
    gdImagePtr out_contrast_img;
    gdImagePtr out_smooth_img;
    
    gdImagePtr out_texture_img;
    char filesout[100];
    int indice;

}PipeArgs;

typedef struct TimeData {
    struct timespec start_contrast;
    struct timespec end_contrast;
    struct timespec start_sepia;
    struct timespec end_sepia;
} TimeData;

double calculate_execution_time(struct timespec start, struct timespec end) {
    //devolve o tempo de execução da thread
    return (end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1.0e9;
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

void *contrast(void *arg) {
    ThreadArgs *threadArg = (ThreadArgs *)arg;

    PipeArgs *pipeArg = malloc(sizeof(PipeArgs) * threadArg->numImagens);
    if (pipeArg == NULL) {
        fprintf(stderr, "Memory allocation failed for pipeArg\n");
        exit(EXIT_FAILURE);
    }

    
    
    int *pipe_fd = threadArg->pipe_fd;
    int *pipe_fd_contrast_to_smooth = threadArg->pipe_fd_contrast_to_smooth;
    char filePipe[100];
    gdImagePtr in_img;
    gdImagePtr out_contrast_img;
    int d = 0;
    char out_file_name[100];



    while (read(pipe_fd[0], &filePipe, sizeof(filePipe)) > 0) {
        struct timespec start_time_contrast, end_time_contrast;
        clock_gettime(CLOCK_MONOTONIC, &start_time_contrast);


        
    
        threadArg->executionTimeStart[d] = calculate_execution_time(threadArg->start_time_total,start_time_contrast);

        printf("Tempo de inicio da imagem %d no estágio 1 ---> %f\n",pipeArg->indice,threadArg->executionTimeStart[d]);
        if (access(out_file_name, F_OK) != -1) {
            printf("%s found\n", out_file_name);
        } else {
        in_img = read_jpeg_file(filePipe);
        

        if (in_img == NULL) {
            fprintf(stderr, "Impossible to read %s image\n", filePipe);
            exit(EXIT_FAILURE);
        }

        out_contrast_img = contrast_image(in_img);

        // Assign the image pointer to the structure member
        pipeArg[d].out_contrast_img = out_contrast_img;
        pipeArg[d].indice = d;
        strcpy(pipeArg[d].filesout, threadArg->filesout[d]);

        sprintf(out_file_name, "%s%s", OLD_IMAGE_DIR, threadArg->filesout[d]);

        threadArg->pipe_fd_contrast_to_smooth = pipe_fd_contrast_to_smooth;
            printf("%s not found\n", out_file_name);
            // Write the structure to the pipe only if the output file doesn't exist
            write(pipe_fd_contrast_to_smooth[1], &pipeArg[d], sizeof(PipeArgs));
            gdImageDestroy(in_img);
        }   

        
       
        clock_gettime(CLOCK_MONOTONIC, &end_time_contrast);
        threadArg->executionTimeImg[d] = calculate_execution_time(start_time_contrast, end_time_contrast);
        printf("Imagem %d estágio 1 ---> %f\n",pipeArg[d].indice,threadArg->executionTimeImg[pipeArg[d].indice]);
        d++;
    }
    close(pipe_fd_contrast_to_smooth[1]);

    
    pthread_exit(NULL);
}




void *smooth(void *arg){
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    

    int *pipe_fd_contrast_smooth=threadArg->pipe_fd_contrast_to_smooth;
    int *pipe_fd_smooth_to_textured=threadArg->pipe_fd_smooth_to_textured;

    gdImagePtr out_contrast_img;
    gdImagePtr out_smoothed_img;
    PipeArgs pipeArg;
    
  
    while(read(pipe_fd_contrast_smooth[0],&pipeArg,sizeof(pipeArg))>0){
        struct timespec start_time_smooth, end_time_smooth;
        clock_gettime(CLOCK_MONOTONIC, &start_time_smooth);


       
        
        
        out_contrast_img=pipeArg.out_contrast_img;
       
        
        out_smoothed_img = smooth_image(pipeArg.out_contrast_img);
        pipeArg.out_smooth_img=out_smoothed_img;
        write(pipe_fd_smooth_to_textured[1],&pipeArg,sizeof(pipeArg));
        threadArg->pipe_fd_smooth_to_textured=pipe_fd_smooth_to_textured;
        
        
        clock_gettime(CLOCK_MONOTONIC, &end_time_smooth);
        double time = calculate_execution_time(start_time_smooth, end_time_smooth)+threadArg->executionTimeImg[pipeArg.indice];
        threadArg->executionTimeImg[pipeArg.indice] = time;
        printf("Imagem %d estágio 2 ---> %f\n",pipeArg.indice,threadArg->executionTimeImg[pipeArg.indice]);
    }
     close(pipe_fd_smooth_to_textured[1]);
    
    pthread_exit(NULL);
}

void *texture(void *arg){
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    
    int *pipe_fd_smooth_to_textured=threadArg->pipe_fd_smooth_to_textured;
    int *pipe_fd_textured_to_sepia=threadArg->pipe_fd_textured_to_sepia;

    gdImagePtr out_smoothed_img;
    gdImagePtr in_textured_img= read_png_file("./paper-texture.png");
    gdImagePtr out_textured_img;

    PipeArgs pipeArg;
    
    while(read(pipe_fd_smooth_to_textured[0],&pipeArg,sizeof(pipeArg))>0){
        struct timespec start_time_textured, end_time_textured;
        clock_gettime(CLOCK_MONOTONIC, &start_time_textured);
        
        
        out_smoothed_img=pipeArg.out_smooth_img;
        out_textured_img = texture_image(out_smoothed_img, in_textured_img);
       
        pipeArg.out_texture_img=out_textured_img;
        write(pipe_fd_textured_to_sepia[1],&pipeArg,sizeof(pipeArg));
        
        threadArg->pipe_fd_textured_to_sepia=pipe_fd_textured_to_sepia;
        
        
        clock_gettime(CLOCK_MONOTONIC, &end_time_textured);
        double time = calculate_execution_time(start_time_textured, end_time_textured)+threadArg->executionTimeImg[pipeArg.indice];
        threadArg->executionTimeImg[pipeArg.indice] = time;
        printf("Imagem %d estágio 3 ---> %f\n",pipeArg.indice,threadArg->executionTimeImg[pipeArg.indice]);
    }
    close(pipe_fd_textured_to_sepia[1]);
    gdImageDestroy(in_textured_img);
    
    pthread_exit(NULL);
}

void *sepia(void *arg){
    ThreadArgs *threadArg = (ThreadArgs *)arg;
    
    int *pipe_fd_textured=threadArg->pipe_fd_textured_to_sepia;
    char *filePipe;
    gdImagePtr out_textured_img;
    gdImagePtr out_sepia_img;
    char **filePipeOut = threadArg->filesout;
    char out_file_name[100];

    PipeArgs pipeArg;
    while(read(pipe_fd_textured[0],&pipeArg,sizeof(pipeArg))>0){
        struct timespec start_time_sepia, end_time_sepia;
        clock_gettime(CLOCK_MONOTONIC, &start_time_sepia);
        
        
        out_textured_img=pipeArg.out_texture_img;
        out_sepia_img = sepia_image(out_textured_img);
        

        char *filePipeOut = pipeArg.filesout;
        sprintf(out_file_name, "%s%s", OLD_IMAGE_DIR, filePipeOut); 
        if(write_jpeg_file(out_sepia_img, out_file_name) == 0){
            fprintf(stderr, "Impossible to write %s image\n", out_file_name);
        }
        clock_gettime(CLOCK_MONOTONIC, &end_time_sepia);
        double time = calculate_execution_time(start_time_sepia, end_time_sepia)+threadArg->executionTimeImg[pipeArg.indice];
        threadArg->executionTimeImg[pipeArg.indice] = time;
        printf("Imagem %d estágio 4 ---> %f\n",pipeArg.indice,threadArg->executionTimeImg[pipeArg.indice]);
        printf("end %d \t %10jd.%09ld\n", pipeArg.indice,end_time_sepia.tv_sec, end_time_sepia.tv_nsec);
        printf("************************************************Guardaste a imagem %d para em %s************************************************\n",pipeArg.indice, out_file_name);
    gdImageDestroy(pipeArg.out_contrast_img);
    gdImageDestroy(pipeArg.out_smooth_img);
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

    threadArg->pipe_fd_contrast_to_smooth=pipe_fd_contrast_to_smooth;
    threadArg->pipe_fd_smooth_to_textured=pipe_fd_smooth_to_textured;
    threadArg->pipe_fd_textured_to_sepia=pipe_fd_textured_to_sepia;
    int *pipe_fd=threadArg->pipe_fd;
    printf("Conseguiste sacar o pipe cá para dentro e declaraste as 4 threads\n");
        pthread_create(&thread_contrast, NULL, contrast, (void *)&threadArg[0]);
        pthread_create(&thread_smooth, NULL, smooth, (void *)&threadArg[0]);
        pthread_create(&thread_textured, NULL, texture, (void *)&threadArg[0]);
        pthread_create(&thread_sepia, NULL, sepia, (void *)&threadArg[0]);
    printf("Conseguiste criar as threads\n");
    for (int i = 0; i < threadArg->numImagens; i++) {
        
        write(pipe_fd[1], threadArg->files[i], MAX_LENGTH * sizeof(char));
    }
    printf("Conseguiste escrever os ficheiros na pipe\n");
    
        close(pipe_fd[1]); // Fecha a escrita no pipe
    

    pthread_join(thread_contrast, NULL);

    printf("****THREAD CONTRAST JOINADA COM SUCESSO****\n");
    pthread_join(thread_smooth, NULL);
    printf("****THREAD SMOOTH JOINADA COM SUCESSO****\n");
    pthread_join(thread_textured, NULL);
    printf("****THREAD TEXTURE JOINADA COM SUCESSO****\n");
    pthread_join(thread_sepia, NULL);
    printf("****THREAD CONTRAST JOINADA COM SUCESSO****\n");

    printf("VIVAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
    //free(pipe_fd);
}

int main(int argc, char *argv[]) {
    //Inicialização das estruturas de tempo
    struct timespec start_time_total, end_time_total;
    
    clock_gettime(CLOCK_MONOTONIC, &start_time_total);
    

    
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
    

    ThreadArgs *threadArg = malloc(sizeof(ThreadArgs)*numImagens);
    if (threadArg == NULL) {
        fprintf(stderr, "Memory allocation failed for threadArg\n");
        exit(EXIT_FAILURE);
    }
    
    

    
    
    char **completeFilePaths = malloc(numImagens * sizeof(char *));
    if (completeFilePaths == NULL) {
        fprintf(stderr, "Memory allocation failed for completeFilePaths\n");
        exit(EXIT_FAILURE);
    }

        threadArg->start_time_total=start_time_total;
        threadArg->numImagens = numImagens;
        threadArg->pipe_fd=pipe_fd;

        for (int j = 0; j < numImagens; j++) {
            completeFilePaths[j] = malloc(MAX_LENGTH * sizeof(char));
            if (completeFilePaths[j] == NULL) {
                fprintf(stderr, "Memory allocation failed for completeFilePaths[%d]\n", j);
                exit(EXIT_FAILURE);
            }
            
            // Junta o diretório fornecido pelo terminal com o nome da imagem
            sprintf(completeFilePaths[j], "%s/%s", argv[1], files[j]);

            // Atribuir o novo caminho completo ao vetor files e filesout(imagens de saída) na estrutura
            threadArg->files[j] = completeFilePaths[j];
            threadArg->filesout[j] = files[j];
            
        }
    


    /* creation of output directories */
    if (create_directory(OLD_IMAGE_DIR) == 0){
        fprintf(stderr, "Impossible to create %s directory\n", OLD_IMAGE_DIR);
        exit(EXIT_FAILURE);
    }

    
    
    printf("O problema está dentro da pipeline\n");
    cria_pipeline(threadArg);//Função que cria as várias threads



    
    clock_gettime(CLOCK_MONOTONIC, &end_time_total);

   
    struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);
   


    
    //Criação do ficheiro timings.txt
    char nomeFicheirorelatorio[100];
    sprintf(nomeFicheirorelatorio, "%stiming_pipeline.txt", OLD_IMAGE_DIR);

    FILE *relatorio = fopen(nomeFicheirorelatorio, "w");
    if (relatorio == NULL) {
        perror("Erro ao criar o arquivo");
        exit(EXIT_FAILURE);
    }else{
    

    for (int i = 0; i < numImagens; i++) {

        fprintf(relatorio, "%s\tstart\t%.3f\n", threadArg->filesout[i], threadArg->executionTimeStart[i]);
        fprintf(relatorio, "%s\tend\t%.3f\n", threadArg->filesout[i], threadArg->executionTimeImg[i]);
    }

    }

    fprintf(relatorio,"total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);

    fclose(relatorio);


    printf("\n\n");
    
    for (int i = 0; i < numImagens; i++) {
        printf("%s\tstart\t%.3f\n", threadArg->filesout[i], threadArg->executionTimeStart[i]);
        printf("%s\tend\t%.3f\n", threadArg->filesout[i], threadArg->executionTimeImg[i]);
        
    }

    

    printf("total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);

   
    printf("total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);
    
    // Free dynamically allocated memory
    free(threadArg);
    for (int i = 0; i < numImagens; i++) {
        free(files[i]);
    }
    
    return 0;
}

