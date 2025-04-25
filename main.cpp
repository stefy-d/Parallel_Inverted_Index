#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <unordered_set>

struct mapper_args {
    int thread_id;                 // id-ul thread-ului
    int n;                         // numarul total de fisiere
    std::vector<std::string> *files; // pointer la vectorul de fisiere
    int *files_index;              // pointer la indexul partajat al fisierelor
    pthread_mutex_t *files_index_mutex; // mutex pentru sincronizarea indexului
    std::vector<std::pair <std::string, int>> words; // dictionarul de cuvinte 
    pthread_barrier_t *barrier; // bariera pentru sincronizarea reducerilor   
};

struct reducer_args {
    int thread_id;  // id-ul thread-ului
    int m, r;  // numarul de mapperi si reduceri
    // lista finala de cuvinte grupate dupa prima litera
    std::map<char, std::vector<std::pair<std::string, std::set<int>>>> *final_list;
    pthread_mutex_t *final_list_mutex; // mutex pentru sincronizarea listei finale
    struct mapper_args *mapper_args; // pointer la thread-urile de tip mapper
    pthread_barrier_t *barrier; // bariera pentru sincronizarea reducerilor
};

void *mapper(void *arg) {
    mapper_args *args = (mapper_args *)arg;

    int current_index;  // indexul curent al fisierului procesat
    while (true) {
 
        // acces sincronizat la files_index
        pthread_mutex_lock(args->files_index_mutex);
        current_index = *(args->files_index); // preiau indexul fisierului la care s-a ajuns
        (*(args->files_index))++; // incrementez indexul pentru urmatorul fisier
        pthread_mutex_unlock(args->files_index_mutex);  // deblochez accesul la indexul fisierului

        // verific daca am ajuns la finalul listei de fisiere
        if (current_index > args->n) {
            break;
        }

        // deschidere fisier
        std::ifstream file(args->files->at(current_index));
        if (!file.is_open()) {
            std::cerr << "Eroare la deschiderea fișierului " << args->files->at(current_index) << std::endl;
            exit(-1);
        }

        std::string word;   // cuvantul curent citit din fisier
        std::unordered_set<std::string> unique_words;   // lista de cuvinte unice din fisier

        // citire cuvinte din fisier
        while (file >> word) {

            // formatez cuvantul astfel incat sa contina doar litere mici
            std::string create_word;
            for (auto c : word) {
                if (isalpha(c)) {
                    create_word += tolower(c);
                }
            }

            // verific daca cuvantul a mai fost procesat din acelasi fisier
            if (unique_words.find(create_word) == unique_words.end()) {
                // adaug cuvantul in lista auxiliara pentru a nu-l mai procesa in viitor din acelasi fisier
                unique_words.insert(create_word); 
                
                // adaug cuvantul si in lista de cuvinte a thread-ului impreuna cu indexul fisierului
                args->words.emplace_back(create_word, current_index);
                
            }
        }

        // inchidere fisier
        file.close();

    }

    pthread_barrier_wait(args->barrier);    // asteptare mapperi
    pthread_exit(NULL); // toate fisierele au fost procesate
}

void *reducer(void *arg) {
    
    reducer_args *args = (reducer_args *) arg;  // argumentele pentru thread-ul de tip reducer

    pthread_barrier_wait(args->barrier);    // asteptare mapperi
    
    // intervalul de litere pe care thread-ul le proceseaza
    int step = 26 / args->r;
    char start, end;
    start = 'a' + args->thread_id * step;   // inceputul de interval

    // verific daca thread-ul este ultimul pentru a sti unde se termina intervalul
    if (args->thread_id != args->r - 1) {       
        end = 'a' + (args->thread_id + 1) * step - 1;  
    } else {
        end = 'z';  // ultimul thread ia toate literele pana la z
    }
    
    // lista auxiliaza pentru cuvintele din intervalul stabilit, grupate dupa prima litera
    std::map<char, std::vector<std::pair<std::string, int>>> grouped_by_first_letter;

    // preiau datele din mapperii primiti ca argument
    mapper_args *mapper_data = (mapper_args *)args->mapper_args;
    
    std::string word;   // cuvantul curent din fiecare lista de cuvinte a mapperilor
    int file_id;        // id-ul fisierului din care provine cuvantul

    // iterez prin fiecare mapper adaugand cuvintele in lista auxiliara, grupate dupa prima litera
    for (int i = 0; i < args->m; i++) {
        for (auto &pair : mapper_data[i].words) {
            //preiau datele despre cuvant si fisier
            word = pair.first;
            file_id = pair.second;

            // verific daca cuvantul este in intervalul de litere al thread-ului
            if (word[0] >= start && word[0] <= end) {
                // adaug cuvantul si inexul in lista auxiliara grupat dupa prima litera
                grouped_by_first_letter[word[0]].emplace_back(word, file_id);   
            }
        }
    }

    // iterez prin fiecare grup de cuvinte al fiecarei litere
    for (auto &letter_group : grouped_by_first_letter) {
        char letter = letter_group.first;   // litera curenta pe care o procesez
        auto &words = letter_group.second;  // lista de cuvinte pentru litera curenta

        // vector in care pentru fiecare cuvant retin toate fisierele in care apare
       std::vector<std::pair<std::string, std::set<int>>> combined_files;

        // iterez prin fiecare cuvant si adaug fisierele in care apare in vectorul final
        for (auto &pair : words) {
            auto find_pair = std::find_if(combined_files.begin(), combined_files.end(),
                           [&pair](const auto &entry) { return entry.first == pair.first; });
            if (find_pair != combined_files.end()) {
                // daca am gasit cuvantul, adaug fisierul in care apare la cele existente
                find_pair->second.insert(pair.second);
            } else {
                // daca nu am gasit cuvantul, adaug cuvantul si fisierul in care apare
                combined_files.emplace_back(pair.first, std::set<int>{pair.second});
            }
        }

        // sortez cuvintele in ordine descrescatoare dupa numarul de fisiere in care apar si apoi alfabetic
        std::sort(combined_files.begin(), combined_files.end(), [](const auto &a, const auto &b) {
            if (a.second.size() != b.second.size()) {
                return a.second.size() > b.second.size();
            }
            return a.first < b.first;
        });

        // adaug cuvintele cu litera curenta sortate in lista finala, blocand accesul altor thread-uri
        pthread_mutex_lock(args->final_list_mutex);
        (*args->final_list)[letter].insert((*args->final_list)[letter].end(), combined_files.begin(), combined_files.end());
        pthread_mutex_unlock(args->final_list_mutex);

        // creez fisierul pentru litera curenta
        std::ofstream out_file(std::string(1, letter) + ".txt");
        if (!out_file.is_open()) {
            std::cerr << "Eroare la crearea fișierului " << letter << ".txt\n";
            exit(-1);
        }

        // scriu fiecare pereche cuvant-fisere in fisier
        for (const auto &pair : combined_files) {
            out_file << pair.first << ":[";
            auto last = std::prev(pair.second.end()); // ultimul fisier pentru cuvant
            
            // scriu fiecare fisier in care apare cuvantul
            for (auto file_index = pair.second.begin(); file_index != pair.second.end(); file_index++) {
                out_file << *file_index; // scriu indexul fisierului

                // adaug spatiu dupa acesta daca nu este ultimul element
                if (file_index != last) {
                    out_file << " ";
                }
            }
            out_file << "]\n";  // inchid perechea cuvant-fisiere
        }

        // am terminat de scris toate cuvintele pentru litera curenta si inchid fisierul
        out_file.close();
        
    }

    // creez fisierele pentru literele care nu au cuvinte si le las goale
    for (char letter = start; letter <= end; letter++) {
        if (grouped_by_first_letter.find(letter) == grouped_by_first_letter.end()) {
            std::ofstream out_file(std::string(1, letter) + ".txt");
            out_file.close();
        }
    }

    pthread_exit(NULL);  
}

int main(int argc, char **argv) {

    if (argc < 4) {
        std::cerr << "Numar insuficient de argumente" << std::endl;
        return 1;
    }

    int m = atoi(argv[1]);
    int r = atoi(argv[2]);

    // deschidere fisier cu nume de fisiere
    std::ifstream file(argv[3]);
    if (!file.is_open()) {
        std::cerr << "Eroare la deschiderea fișierului " << argv[3] << std::endl;
        return -1;
    }

    // citire numar de fisiere
    int n;
    file >> n;

    // citirea numelor fisierelor
    std::vector<std::string> files(n + 1);
    for (int i = 1; i <= n; i++) {
        file >> files[i];
    }

    // inchidere fisier
    file.close();

    pthread_t mapper_threads[m];     // toate thread-urile in număr de m
    pthread_t reducer_threads[r];    // toate thread-urile in număr de r
    struct mapper_args mapper_args[m]; // argumentele pentru thread-urile de tip mapper
    struct reducer_args reducer_args[r]; // argumentele pentru thread-urile de tip reducer
    int result;                    // rezultatul crearii/asteptării thread-ului

    int initial_index = 1; // indexul la care s-a ajuns in vectorul de fisiere
    pthread_mutex_t files_index_mutex; // mutex pentru accesul la files_index
    pthread_mutex_init(&files_index_mutex, NULL);

    pthread_mutex_t final_list_mutex; // mutex pentru accesul comun al reducerilor la final_list
    pthread_mutex_init(&final_list_mutex, NULL);

    pthread_barrier_t barrier; // bariera pentru asteptarea mapperilor
    pthread_barrier_init(&barrier, NULL, m + r);

    std::map<char, std::vector<std::pair<std::string, std::set<int>>>> final_list; // lista finala de cuvinte

    // creare thread-uri
    for (int i = 0; i < m + r; i++) {
        
        // creez thread-uri de tip mapper
        if (i < m) {
            mapper_args[i].thread_id = i;
            mapper_args[i].n = n;
            mapper_args[i].files = &files;
            mapper_args[i].files_index = &initial_index;
            mapper_args[i].files_index_mutex = &files_index_mutex;
            mapper_args[i].words = {};
            mapper_args[i].barrier = &barrier;

            result = pthread_create(&mapper_threads[i], NULL, mapper, &mapper_args[i]);
            if (result) {
                std::cerr << "Eroare la crearea thread-ului " << i << std::endl;
                exit(-1);
            }

        } else {    // creez thread-uri de tip reducer
            reducer_args[i - m].thread_id = i - m;
            reducer_args[i - m].m = m;
            reducer_args[i - m].r = r;
            reducer_args[i - m].final_list = &final_list;
            reducer_args[i - m].final_list_mutex = &final_list_mutex;
            reducer_args[i - m].mapper_args = mapper_args;
            reducer_args[i - m].barrier = &barrier;

            result = pthread_create(&reducer_threads[i - m], NULL, reducer, &reducer_args[i - m]);
            if (result) {
                std::cerr << "Eroare la crearea thread-ului " << i << std::endl;
                exit(-1);
            }
            
        }
    }

    // asteptare thread-uri
    for (int i = 0; i < m + r; i++) {
        if (i < m) {    // asteptare thread-uri de tip mapper
            pthread_join(mapper_threads[i], NULL);
        } else {    // asteptare thread-uri de tip reducer
            pthread_join(reducer_threads[i - m], NULL);
        }
    }

    // distrugerea mutexurilor
    pthread_mutex_destroy(&files_index_mutex);
    pthread_mutex_destroy(&final_list_mutex);

    // distrugerea barierei
    pthread_barrier_destroy(&barrier);

    return 0;
}
