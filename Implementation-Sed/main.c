/**
 * @file sem.c
 * @author Pavol Kmeť
 * @date 10 December 2016
 *
 * @brief Paralelná implementácia príkazu sed
 *
 * @details V jazyku C/C++ implementujte vlastnú verziu príkazu sed. Implementácia musí využiť vlákna na urýchlenie činnosti aplikácie,
 * ak táto je spustená na viacjadrovom systéme. Vstupom je vzorka textu, ktorá sa má nahradiť vo všetkých riadkoch všetkých zadaných
 * súborov a reťazec, ktorým ju treba nahradiť. Štandardne program kopíruje všetky riadky všetkých zadaných súborov na štandardný výstup,
 * pričom výstup je generovaný podľa zadaných prepínačov. Program by mal rozpoznávať minimálne nasledujúce prepínače:
 *
 * s – vykoná náhradu na mieste, t.j. nevypisuje výsledok na štandardný výstup, ale výsledkom bude nahradenie vzorky priamo v zadanom súbore.
 * i – ignoruje veľké/malé písmená pri porovnávaní riadkov so vzorkou
 *
 */

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>

#define FLAG_S "-s"
#define FLAG_I "-i"
#define FLAG_SECVENTIONAL "-secventional"

#define I_DISABLED flagI == 0
#define I_ENABLED flagI == 1
#define S_DISABLED flagS == 0
#define S_ENABLED flagS == 1

#define INPUT_SIZE 4096

#define clearInput memset(input, '\0', INPUT_SIZE);

/**
 * @var wordOne
 *
 * @brief premenna,do ktorej sa načíta reťazec, ktorý budeme hľadať v súbore
 */
char *wordOne;

/**
 * @var wordTwo
 *
 * @brief premenna,do ktorej sa načíta reťazec, ktorý budeme meniť za hľadaný reťazec
 */
char *wordTwo;

/**
 * @var flagI
 *
 * @brief premenná,do ktorej je načítaný prepínač -i
 */
int flagI = 0;

/**
 * @var flagS
 *
 * @brief premenná,do ktorej je načítaný prepínač -s
 */
int flagS = 0;

/**
 * @var isSecventional, ak je nastavený na 1, tak sa príkaz vykonáva sekvenčne
 *
 * @brief premenná,do ktorej je načítaný prepínač -secventional
 */
int isSecventional = 0;

/**
 * @var mutex
 *
 * @brief mutext, ktorý je používaný pri výpise. Tz. aby mal výpis dobré poradie tak sa mutext uzamkne, dokým sa neskončí vypisovanie a po skončení sa odomkne.
 */
pthread_mutex_t mutex;

int usage();
void renameFile(const char *originalName, const char *newName);
char *toLowerCase(char *word);
char *replace_str(const char *original, const char *pattern, const char *replacement);
void *thread(void *inputFile);

/**
 * Metóda, ktorá vypisuje ako pracovať so semetrálkou.
 *
 * @return int, s chybovým číslom
 */
int usage() {
    fprintf(stderr, "USAGE: \tsed [FLAGS] <WORD> <REPLACEMENT> <FILES ...>\n");
    fprintf(stderr, "FLAGS:\n\t-i - Ignore case \n\t-s - No printing\n");
    fprintf(stderr, "WORD: \n\tWord for we will be searching for\n");
    fprintf(stderr, "REPLACEMENT: \n\tReplacemenet word which replace WORD in files.\n");
    fprintf(stderr, "FILES: \n\tFiles where WORD will be searching.\n");
    return 1;
}

/**
 * Metóda, ktorá sa slúži na premenovanie súboru. Ak sa premenovanie súboru nepodarí vypíše chybu z 'errno'.
 *
 * @param originalName reťazec, pôvodné meno súboru
 * @param newName reťazec, nové meno súboru
 */
void renameFile(const char *originalName, const char *newName) {
    if (rename(originalName, newName) == 0) {
        printf("Result is in file with name '%s'.\n\n", newName);
    } else {
        if (remove(originalName) != 0) {
            fprintf(stderr, "File deleting error: %s\n", strerror(errno));
        }
        fprintf(stderr, "File renaming error: %s\n", strerror(errno));
    }
}

/**
 * Zoberie vstupný paramater a premení ho na malé znaky.
 *
 * @param word reťazec, ktorý vstupuje do funkcie a bude upravený na malé znaky
 */
char *toLowerCase(char *word) {
    
    long length = strlen(word);
    
    for(int i = 0; i < length; i++) {
        word[i] = tolower(word[i]);
    }
    
    return word;
}

/**
 * Metóda, ktorá hľadá v 'originalString' reťazci, 'oldString' reťazec a nahrádza ho 'newString' reťazcom.
 *
 * @param original prehľadávaný reťazec
 * @param pattern reťazec, ktorý je hľadaný
 * @param replacement reťazec, ktorý sa nahradí 'oldString'
 *
 * @return correctedString reťazec, s nahradením hľadaným reťazcom
 */
char *replace_str(const char *original, const char *pattern, const char *replacement) {
    
    char *corrected, *r;
    const char *p, *q;
    
    size_t patternLength = strlen(pattern);
    size_t replacementLength = strlen(replacement);
    size_t count, correctedLength;
    
    if (patternLength != replacementLength) {
        
        for (count = 0, p = original; (q = strstr(p, pattern)) != NULL; p = q + patternLength) {
            count++;
        }
        
        correctedLength = p - original + strlen(p) + count * (replacementLength - patternLength);
    } else {
        correctedLength = strlen(original);
    }
    
    if ((corrected = malloc(correctedLength + 1)) == NULL) {
        return NULL;
    }
    
    for (r = corrected, p = original; (q = strstr(p, pattern)) != NULL; p = q + patternLength) {
        ptrdiff_t length = q - p;
        memcpy(r, p, length);
        r += length;
        memcpy(r, replacement, replacementLength);
        r += replacementLength;
    }
    
    strcpy(r, p);

    return corrected;
}

/**
 * Hlavná metóda programu, ktorá zisťuje aké prepínače boli použité a podľa nich spracuje vstupny súbor.
 *
 * @param inputFile názvo súboru, s ktorým sa pracuje vo vlákne
 */
void *thread(void *inputFile) {
    
    char input[INPUT_SIZE], *tempFileName;
    char *inputFileName = (char *)inputFile;
    
    asprintf(&tempFileName, "%s.TEMPORARY_FILE_%d.txt", inputFileName, getpid());
    
    FILE *file;
    file = fopen(inputFileName,"r");
    
    if (file == NULL) {
        fprintf(stderr, "File opening '%s' error: %s\n", inputFileName, strerror(errno));
        return NULL;
    }
    
    if (I_ENABLED && S_DISABLED) {
        pthread_mutex_lock(&mutex);
        
        while (!feof(file)) {
            fgets(input, INPUT_SIZE, file);
            printf("%s", replace_str(toLowerCase(input), toLowerCase(wordOne), wordTwo));
            clearInput
        }
        
        printf("\n\n");
        pthread_mutex_unlock(&mutex);
    }
    
    if (I_DISABLED && S_ENABLED) {
        
        FILE *tempFile;
        tempFile = fopen(tempFileName,"w+");
        
        while (!feof(file)) {
            fgets(input, INPUT_SIZE, file);
            fputs(replace_str(input, wordOne, wordTwo), tempFile);
            clearInput
        }
        
        fclose(tempFile);
        renameFile(tempFileName, inputFileName);
    }
    
    if (I_DISABLED && S_DISABLED) {
        pthread_mutex_lock(&mutex);
        
        while (!feof(file)) {
            fgets(input, INPUT_SIZE, file);
            printf("%s", replace_str(input, wordOne, wordTwo));
            clearInput
        }
        
        printf("\n\n");
        pthread_mutex_unlock(&mutex);
    }
    
    if (I_ENABLED && S_ENABLED) {
        
        FILE *tempFile;
        tempFile = fopen(tempFileName,"w+");
        
        while (!feof(file)) {
            fgets(input, INPUT_SIZE, file);
            fputs(replace_str(toLowerCase(input), toLowerCase(wordOne), wordTwo), tempFile);
            clearInput
        }
        
        fclose(tempFile);
        renameFile(tempFileName, inputFileName);
    }
    
    rewind(file);
    fclose(file);
    
    if (isSecventional == 0) {
        pthread_exit(NULL);
    }

    return 0;
}

/**
 * Hlavná metóda, inicializuje pomocné premenné, vytvára vlákna.
 */
int main(int argc, char *argv[]) {
    
    int i, numberOfFiles, arguments = 1;

    if (argc < 3) {
        fprintf(stderr, "Not enough arguments.\n");
        return usage();;
    }
    
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], FLAG_SECVENTIONAL) == 0) {
            isSecventional = 1;
            arguments++;
            continue;
        }
        if (strcmp(argv[i], FLAG_I) == 0) {
            flagI = 1;
            arguments++;
            continue;
        }
        if (strcmp(argv[i], FLAG_S) == 0) {
            flagS = 1;
            arguments++;
            continue;
        }
    }
    
    if ((wordOne = argv[arguments++]) == NULL) {
        fprintf(stderr, "Original word not entered.\n");
        return usage();;
    }
    
    if ((wordTwo = argv[arguments++]) == NULL) {
        fprintf(stderr, "Replacement word not entered.\n");
        return usage();;
    }
    
    if ((numberOfFiles = argc - arguments) <= 0) {
        fprintf(stderr, "No FILES entered.\n");
        return usage();;
    }
    
    if (isSecventional == 1) {
        
        for (i = 0; i < numberOfFiles; i++) {
            thread((void *)argv[i+arguments]);
        }
        
    } else {
        pthread_mutex_init(&mutex, NULL);
        
        pthread_t *threads = malloc(sizeof(pthread_t) * (numberOfFiles + 1));
        
        for (i = 0; i < numberOfFiles; i++) {
            pthread_create(&threads[i], NULL, thread, (void *)argv[i+arguments]);
        }
        for (i = 0; i < numberOfFiles; i++) {
            pthread_join(threads[i], NULL);
        }
        
        pthread_mutex_destroy(&mutex);
    }
    
    pthread_exit(NULL);
    
    return 0;
}

