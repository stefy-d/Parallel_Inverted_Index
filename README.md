<div align="left">

# **Parallel Inverted Index - Map-Reduce** 
### **Manea Ștefania-Delia**
<br>

## **```Prezentare generală ```**

        Se creează un număr de „m” thread-uri mapper și un număr de „r” thread-uri reducer. 
    Fiecare mapper prelucrează un număr variabil de fișiere deoarece împărțirea 
    acestora se face dinamic, thread-ul preluând un nou fișier în momentul în care a 
    terminat de procesat pe cel curent. Mapperii procesează cuvintele din fișier 
    conform cerinței și le adaugă în lista proprie thread-ului. După ce toți mapperii 
    au terminat, reducerii își încep activitatea. 
        Ideea pe care am abordat-o pentru activitatea reducer-ilor este următoarea: 
    Fiecare thread reducer primește un interval de litere de care să se ocupe și va 
    procesa cuvintele ce încep cu aceste litere. Preia din listele mapperilor cuvintele 
    ce încep cu literele alocate, iar acestea sunt și ele grupate după prima literă. 
    Apoi practic, prelucrez câte o literă pe rând, pentru fiecare cuvânt ce începe 
    cu acea literă, adun toate aparițiile cuvântului în toate fișierele și le sortez 
    conform cerinței. La final, creez fișierul de output pentru litera curentă și 
    scriu în el ce am obținut mai devreme. Pentru literele cu care nu începe niciun 
    cuvânt, creez fișierul de output și îl las gol.

 <br>   

### **```Main-ul:```**

După preluarea argumentelor din linia de comadă, creez un vector pentru a citi 
toate fișierele ce urmează să fie procesate. Apoi, într-un singur „for” creez 
thread-urile mapper în număr de „m” și reducer în număr de „r”.

La creare, fiecare thread primește un id unic și un pointer la structura de date 
ce conține informațiile necesare pentru a-și îndeplini task-urile și funcția 
necesară. În cazul mapper-ului, acesta primește un pointer la structura de date 
ce conține informațiile necesare pentru a citi fișierele și a le procesa. În cazul 
reducer-ului, acesta primește un pointer la structura de date ce conține 
informațiile necesare pentru a procesa datele obținute din activitatea tuturor mapper-ilor. 

Tot în main se așteaptă ca toate thread-urile să își încheie activitatea.

<br>

### **```Structurile de date:```**
### *Mapper_args:* 
- **id**: id-ul thread-ului
- **n**: numărul de fișiere ce urmează să fie procesate
- **files**: vector de string-uri ce conține numele fișierelor ce urmează să fie procesate
- **files_index**: indexul fișierului la care s-a ajuns în procesare
- **files_index_mutex**: mutex pentru a asigura accesul unui singur thread la files_index
- **words**: vector de string-uri ce conține cuvintele procesate de mapper-ul respectiv
- **barrier**: bariera pentru a asigura sincronizarea între mapperi și reduceri

### *Reducer_args:*
- **id**: id-ul thread-ului
- **m**: numărul de mapperi
- **r**: numărul de reduceri
- **final_list**: vector de perechi de string-uri și int-uri ce conține cuvintele 
și indexul fiecărui fișier în care apar, grupate alfabetic
- **final_list_mutex**: mutex pentru a asigura accesul unui singur thread la final_list
- **mapper_args**: vector de structuri de date ce conțin informațiile necesare 
pentru a procesa datele obținute din activitatea tuturor mapper-ilor
- **barrier**: bariera pentru a asigura sincronizarea între mapperi și reduceri

<br>

### **```Funcțiile:```**

### *Mapper:*

În această funcție am un „while” ce rulează până când toate fișierele au fost 
procesate. În fiecare iterație, se blochează mutexul asociat fișierelor pentru a 
extrage valoarea corectă a indexului fișierului la care s-a ajuns, se incrementează 
această valoare și apoi se eliberează mutexul pentru a putea fi preluat și de 
alt thread. Verific dacă indexul este valid, și dacă nu este, opresc ciclarea. 
Dacă indexul este valid, citesc fiecare cuvânt din fișierul respectiv, îl formatez 
conform cerinței și îl adaug la lista temporară de cuvinte a fișierului curent 
pentru a nu mai fi procesat dacă mai apare în acest fișier și la lista finală de 
cuvinte a mapper-ului respectiv doar dacă este un cuvânt nou.

### *Reducer:*

La început, aștept ca toate thread-urile mapper să își încheie activitatea. 
Apoi, pentru ca fiecare thread să aibă un volum de muncă cât de cât echilibrat, 
am decis să atribui fiecărui thread un interval de litere de care să se ocupe. 
Astfel, fiecare thread va avea un număr de 26 / r litere, mai puțin ultimul thread 
care le va avea pe toate rămase până la „z”. Apoi, fiecare thread va parcurge 
listele obținute de mapperi și va forma o listă proprie de cuvinte ce încep cu 
literele alocate, grupate după prima literă.

După asta, pentru fiecare literă alocată, iterez prin cuvintele ce încep cu 
aceasta și pun laolaltă toate aparițiile cuvântului respectiv în toate fișierele, 
într-un vector. După ce am agregat fișierele pentru fiecare cuvânt în parte, sortez 
vectorul cum e în cerință și adaug ce am obținut la lista partajată de reduceri.

De asemenea, creez și fișierul de output pentru litera curentă și scriu în el 
ce am obținut mai devreme.

După ce termin de preocesat intervalul de cuvinte, pentru literele cu care nu 
începe niciun cuvânt, creez fișierul de output și îl las gol.

