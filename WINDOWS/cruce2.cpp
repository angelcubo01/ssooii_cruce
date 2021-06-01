#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "cruce2.h"

//DEFINIMOS LOS TIPOS DE PUNTERO A FUNCIONES
typedef int (*TIPO_CRUCE_Inicio)(int, int);
typedef int (*TIPO_CRUCE_Fin)(void);
typedef int (*TIPO_CRUCE_Gestor_inicio)(void);
typedef int (*TIPO_CRUCE_Pon_semAforo)(int, int);
typedef int (*TIPO_CRUCE_Nuevo_proceso)(void);
typedef struct posiciOn (*TIPO_CRUCE_Inicio_coche)(void);
typedef struct posiciOn (*TIPO_CRUCE_Avanzar_coche)(struct posiciOn);
typedef int (*TIPO_CRUCE_Fin_coche)(void);
typedef struct posiciOn (*TIPO_CRUCE_Inicio_peatOn)(void);
typedef struct posiciOn (*TIPO_CRUCE_Avanzar_peatOn)(struct posiciOn);
typedef int (*TIPO_CRUCE_Fin_peatOn)(void);
typedef int (*TIPO_CRUCE_Pausa)(void);
typedef int (*TIPO_CRUCE_Pausa_coche)(void);
typedef int (*TIPO_CRUCE_Refrescar)(void);
typedef void (*TIPO_Pon_error)(const char *);
//----------------------------------------------------------------------
struct variables
{
    HANDLE cicloSem;
    DWORD idCicloSem;
    HANDLE crearPeaton;
    DWORD idcrearPeaton;
    HANDLE crearCoche;
    DWORD idcrearCoche;
    //SEMAFOROS
    HANDLE semCrearHilos;
    HANDLE semMuertePeatones;
    HANDLE tableroPeaton[867];
    HANDLE tableroCoche[41];
} var;
struct funciones
{ //PARA QUE FUNCIONEN CON LA DLL
    HINSTANCE biblioteca;
    /////////FUNCIONES//////////////////////////////
    TIPO_CRUCE_Inicio CRUCE_inicio;
    TIPO_CRUCE_Fin CRUCE_fin;
    TIPO_CRUCE_Gestor_inicio CRUCE_gestor_inicio;
    TIPO_CRUCE_Pon_semAforo CRUCE_pon_semAforo;
    TIPO_CRUCE_Nuevo_proceso CRUCE_nuevo_proceso;
    TIPO_CRUCE_Inicio_coche CRUCE_inicio_coche;
    TIPO_CRUCE_Avanzar_coche CRUCE_avanzar_coche;
    TIPO_CRUCE_Fin_coche CRUCE_fin_coche;
    TIPO_CRUCE_Inicio_peatOn CRUCE_inicio_peatOn;
    TIPO_CRUCE_Avanzar_peatOn CRUCE_avanzar_peatOn;
    TIPO_CRUCE_Fin CRUCE_fin_peatOn;
    TIPO_CRUCE_Pausa CRUCE_pausa;
    TIPO_CRUCE_Pausa_coche CRUCE_pausa_coche;
    TIPO_CRUCE_Refrescar CRUCE_refrescar;
    TIPO_Pon_error pon_error;
    ////////////////////////////////////////////////
} typedef funciones;

//Funciones DLL y semaforos
int waitSemaforo(HANDLE semaforo, int cant, int tiempo);
int signalSemaforo(HANDLE semaforo, int cant);
int iniciarDLL(funciones *v);

//Crear Tableros de semaforos
int crearTableroPeaton(void);
int crearTableroCoche(void);

//Modificar Tableros
int reservarPosicionPeaton(struct posiciOn);
int liberarPosicionPeaton(struct posiciOn);
int reservarPosicionCoche(int pos);
int liberarPosicionCoche(int pos);
int posSemaforo(struct posiciOn);
int semPeatonesROJO(int semaforo);
int semPeatonesVERDE(int semaforo);
int accedeZonaCriticaCRUCE(void);
int liberaZonaCriticaCRUCE(void);

//Funciones de los distintos hilos
DWORD WINAPI cicloSemaforico(LPVOID v);
DWORD WINAPI conduceCoche(LPVOID v);
DWORD WINAPI andaPeaton(LPVOID v);

int main(int argc, char const *argv[])
{
    // COMPROBACION INICIAL - PASO POR LINEA DE ORDENES -
    int velocidad, nHilos;
    if (argc != 3)
    {
        printf("ERROR!! : Argumentos pasados incorrectamente\n");
        printf("Se deben pasar 2 enteros (la velocidad y el num max de hilos)\n\n");
        return (-1);
    }
    else
    {
        nHilos = atoi(argv[1]);
        if (nHilos <= 3 || nHilos > 49)
        {
            printf("EL NUMERO DE HILOS ES INCORRECTO!!\n");
            return -1;
        }
        velocidad = atoi(argv[2]);
        if (velocidad < 0)
        {
            printf("ERROR LA VELOCIDAD TIENE QUE SER UN ENTERO POSITIVO!!\n");
            return -1;
        }
    }

    funciones f;
    int cochePeaton;

    f.biblioteca = LoadLibraryA("cruce2.dll");
    if (f.biblioteca == NULL)
    {
        PERROR("\nError dll");
        return -1;
    }

    if (iniciarDLL(&f) == -1)
    {
        PERROR("\nError al cargar dll");
        Sleep(10000);
        FreeLibrary(f.biblioteca);
        exit(-1);
    }
    if (crearTableroPeaton() == -1)
    {
        PERROR("Error creando tableroPeaton");
        Sleep(10000);
        FreeLibrary(f.biblioteca);
        exit(-1);
    }
    if (crearTableroCoche() == -1)
    {
        PERROR("Error creando tableroCoche");
        Sleep(10000);
        FreeLibrary(f.biblioteca);
        exit(-1);
    }

    f.CRUCE_inicio(velocidad, nHilos);

    //Entra el hilo por la función CicloSemaforico
    var.cicloSem = CreateThread(NULL, 0, &cicloSemaforico, &f, 0, &(var.idCicloSem));
    if (var.cicloSem == NULL)
    {
        PERROR("Error en hilo cicloSemaforico");
        Sleep(10000);
        FreeLibrary(f.biblioteca);
        exit(-1);
    }

    //Semaforo que controla el numero de hilos
    var.semCrearHilos = CreateSemaphore(NULL, nHilos - 2, nHilos - 2, NULL);
    if (var.semCrearHilos == NULL)
    {
        PERROR("Error en semaforoCrearHilos");
        Sleep(10000);
        FreeLibrary(f.biblioteca);
        exit(-1);
    }

    //Semaforo que hace atómica la muerte de los peatones
    var.semMuertePeatones = CreateSemaphore(NULL, 1, 1, NULL);
    if (var.semMuertePeatones == NULL)
    {
        PERROR("Error en semMuertePeatones");
        Sleep(10000);
        FreeLibrary(f.biblioteca);
        exit(-1);
    }

    //Bucle infinito creando hilos
    while (1)
    {
        waitSemaforo(var.semCrearHilos, 1, INFINITE);

        cochePeaton = f.CRUCE_nuevo_proceso();

        if (cochePeaton == PEAToN)
        {
            //Entra el hilo por la función andaPeaton
            var.crearPeaton = CreateThread(NULL, 0, &andaPeaton, &f, 0, &(var.idcrearPeaton));
            if (var.crearPeaton == NULL)
            {
                PERROR("Error en hilo crearPeaton");
                Sleep(10000);
                FreeLibrary(f.biblioteca);
                exit(-1);
            }
        }
        else if (cochePeaton == COCHE)
        {
            var.crearCoche = CreateThread(NULL, 0, &conduceCoche, &f, 0, &(var.idcrearCoche));
            if (var.crearCoche == NULL)
            {
                PERROR("Error en hilo crearCoche");
                Sleep(10000);
                FreeLibrary(f.biblioteca);
                exit(-1);
            }
        }
    }

    return -1;
}
int crearTableroPeaton(void)
{

    int x, y;
    for (x = 0; x <= 50; x++)
    {
        for (y = 0; y <= 16; y++)
        {
            var.tableroPeaton[y * 51 + x] = CreateSemaphore(NULL, 1, 1, NULL);
            if (var.tableroPeaton[y * 51 + x] == NULL)
            {
                return -1;
            }
        }
    }
    return 0;
}

int posSemaforo(struct posiciOn pos)
{
    if (pos.x >= 0 && pos.x <= 50 && pos.y >= 0 && pos.y <= 16)
    {
        return (pos.y * 51 + pos.x);
    }
    else
        return (-1);
}

int iniciarDLL(funciones *f)
{
    f->CRUCE_inicio = (TIPO_CRUCE_Inicio)GetProcAddress(f->biblioteca, "CRUCE_inicio");
    if (f->CRUCE_inicio == NULL)
    {
        PERROR("\nError asignando CRUCE_inicio");
        return -1;
    }
    f->CRUCE_avanzar_coche = (TIPO_CRUCE_Avanzar_coche)GetProcAddress(f->biblioteca, "CRUCE_avanzar_coche");
    if (f->CRUCE_avanzar_coche == NULL)
    {
        PERROR("\nError asignando CRUCE_avanzar_coche");
        return -1;
    }
    f->CRUCE_avanzar_peatOn = (TIPO_CRUCE_Avanzar_peatOn)GetProcAddress(f->biblioteca, "CRUCE_avanzar_peatOn");
    if (f->CRUCE_avanzar_peatOn == NULL)
    {
        PERROR("\nError asignando CRUCE_avanzar_peaton");
        return -1;
    }
    f->CRUCE_fin = (TIPO_CRUCE_Fin)GetProcAddress(f->biblioteca, "CRUCE_fin");
    if (f->CRUCE_fin == NULL)
    {
        PERROR("\nError asignando CRUCE_fin");
        return -1;
    }
    f->CRUCE_fin_coche = (TIPO_CRUCE_Fin_coche)GetProcAddress(f->biblioteca, "CRUCE_fin_coche");
    if (f->CRUCE_fin_coche == NULL)
    {
        PERROR("\nError asignando CRUCE_fin_coche");
        return -1;
    }
    f->CRUCE_fin_peatOn = (TIPO_CRUCE_Fin_peatOn)GetProcAddress(f->biblioteca, "CRUCE_fin_peatOn");
    if (f->CRUCE_fin_peatOn == NULL)
    {
        PERROR("\nError asignando CRUCE_fin_peaton");
        return -1;
    }
    f->CRUCE_gestor_inicio = (TIPO_CRUCE_Gestor_inicio)GetProcAddress(f->biblioteca, "CRUCE_gestor_inicio");
    if (f->CRUCE_gestor_inicio == NULL)
    {
        PERROR("\nError asignando CRUCE_gestor_inicio");
        return -1;
    }
    f->CRUCE_inicio_coche = (TIPO_CRUCE_Inicio_coche)GetProcAddress(f->biblioteca, "CRUCE_inicio_coche");
    if (f->CRUCE_inicio_coche == NULL)
    {
        PERROR("\nError asignando CRUCE_inicio_coche");
        return -1;
    }
    f->CRUCE_inicio_peatOn = (TIPO_CRUCE_Inicio_peatOn)GetProcAddress(f->biblioteca, "CRUCE_nuevo_inicio_peatOn");
    if (f->CRUCE_inicio_peatOn == NULL)
    {
        PERROR("\nError asignando CRUCE_inicio_peatOn");
        return -1;
    }
    f->CRUCE_nuevo_proceso = (TIPO_CRUCE_Nuevo_proceso)GetProcAddress(f->biblioteca, "CRUCE_nuevo_proceso");
    if (f->CRUCE_nuevo_proceso == NULL)
    {
        PERROR("\nError asignando CRUCE_nuevo_proceso");
        return -1;
    }
    f->CRUCE_pon_semAforo = (TIPO_CRUCE_Pon_semAforo)GetProcAddress(f->biblioteca, "CRUCE_pon_semAforo");
    if (f->CRUCE_pon_semAforo == NULL)
    {
        PERROR("\nError asignando CRUCE_pon_semaforo");
        return -1;
    }
    f->CRUCE_pausa = (TIPO_CRUCE_Pausa)GetProcAddress(f->biblioteca, "pausa");
    if (f->CRUCE_pausa == NULL)
    {
        PERROR("\nError asignando CRUCE_pausa");
        return -1;
    }
    f->CRUCE_pausa_coche = (TIPO_CRUCE_Pausa_coche)GetProcAddress(f->biblioteca, "pausa_coche");
    if (f->CRUCE_pausa_coche == NULL)
    {
        PERROR("\nError asignando CRUCE_avanzar_coche");
        return -1;
    }
    f->CRUCE_refrescar = (TIPO_CRUCE_Refrescar)GetProcAddress(f->biblioteca, "refrescar");
    if (f->CRUCE_refrescar == NULL)
    {
        PERROR("\nError asignando CRUCE_refrescar");
        return -1;
    }
    f->pon_error = (TIPO_Pon_error)GetProcAddress(f->biblioteca, "pon_error");
    if (f->pon_error == NULL)
    {
        PERROR("\nError asignando pon_error");
        return -1;
    }
    return 0;
}

DWORD WINAPI cicloSemaforico(LPVOID v)
{
    int i;
    funciones *func;
    funciones f;
    func = (funciones *)v;
    f = *func;

    int posSemaforo_C1 = 6, posSemaforo_C2 = 31;
    reservarPosicionCoche(14);
    reservarPosicionCoche(15); //Para que no atropellen a los peatones que pasan por el paso P1
    reservarPosicionCoche(16);

    reservarPosicionCoche(posSemaforo_C1); //Para que los coches no se salten los semaforos
    reservarPosicionCoche(posSemaforo_C2);
    f.CRUCE_gestor_inicio();

    semPeatonesROJO(SEM_P2);

    while (1)
    {
        //FASE 1:
        f.CRUCE_pon_semAforo(SEM_C2, ROJO);
        //--------------------------------------------//
        semPeatonesROJO(SEM_P1);
        f.CRUCE_pon_semAforo(SEM_P1, ROJO);
        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_C1, VERDE);
        liberarPosicionCoche(posSemaforo_C1);
        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_P2, VERDE);
        semPeatonesVERDE(SEM_P2);
        //--------------------------------------------//
        for (i = 0; i < 6; i++)
        {
            f.CRUCE_pausa();
        }
        //------------------AMARILLO---------------------//
        reservarPosicionCoche(posSemaforo_C1);
        f.CRUCE_pon_semAforo(SEM_C1, AMARILLO);
        for (i = 0; i < 2; i++)
        {
            f.CRUCE_pausa();
        }
        accedeZonaCriticaCRUCE();
        liberaZonaCriticaCRUCE();
        //------------------FIN AMARILLO-----------------//

        //FASE 2:
        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_C1, ROJO);
        //--------------------------------------------//
        semPeatonesROJO(SEM_P2);
        f.CRUCE_pon_semAforo(SEM_P2, ROJO);
        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_C2, VERDE);
        liberarPosicionCoche(posSemaforo_C2);
        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_P1, ROJO);
        //--------------------------------------------//
        for (i = 0; i < 9; i++)
        {
            f.CRUCE_pausa();
        }
        //------------------AMARILLO---------------------//
        reservarPosicionCoche(posSemaforo_C2);
        f.CRUCE_pon_semAforo(SEM_C2, AMARILLO);
        for (i = 0; i < 2; i++)
        {
            f.CRUCE_pausa();
        }
        accedeZonaCriticaCRUCE();
        liberaZonaCriticaCRUCE();
        //------------------FIN AMARILLO-----------------//

        //FASE 3
        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_C1, ROJO);
        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_C2, ROJO);
        //--------------------------------------------//

        f.CRUCE_pon_semAforo(SEM_P1, VERDE);
        semPeatonesVERDE(SEM_P1);

        //--------------------------------------------//
        f.CRUCE_pon_semAforo(SEM_P2, ROJO);
        //--------------------------------------------//
        for (i = 0; i < 12; i++)
        {
            f.CRUCE_pausa();
        }
    }
    return -1;
}

int waitSemaforo(HANDLE semaforo, int cant, int tiempo)
{
    int i, retrn;
    if (tiempo == INFINITE)
    {
        for (i = 0; i < cant; i++)
        {
            if (WaitForSingleObject(semaforo, INFINITE) == WAIT_FAILED)
            {
                PERROR("Error wait semaforo INF");
                Sleep(10000);
                exit(-1);
            }
        }
    }
    else
    {
        for (i = 0; i < cant; i++)
        {
            if (WaitForSingleObject(semaforo, tiempo) == WAIT_FAILED)
            {
                PERROR("Error wait semaforo");
                Sleep(10000);
                exit(-1);
            }
        }
    }

    return 0;
}
int signalSemaforo(HANDLE semaforo, int cant)
{
    if (!ReleaseSemaphore(semaforo, cant, NULL))
    {

        PERROR("Error signal semaforo");
        Sleep(10000);
        exit(-1);
    }
    return 0;
}
DWORD WINAPI andaPeaton(LPVOID v)
{
    funciones *func;
    funciones f;
    func = (funciones *)v;
    f = *func;

    struct posiciOn actualPeaton, antiguoPeaton;
    struct posiciOn sigPeaton;
    actualPeaton = f.CRUCE_inicio_peatOn();
    reservarPosicionPeaton(actualPeaton);
    sigPeaton = f.CRUCE_avanzar_peatOn(actualPeaton);

    while (1)
    {
        reservarPosicionPeaton(sigPeaton);
        antiguoPeaton = actualPeaton;
        actualPeaton = sigPeaton;
        sigPeaton = f.CRUCE_avanzar_peatOn(actualPeaton);
        f.CRUCE_pausa();
        if (sigPeaton.y >= 0) {
            liberarPosicionPeaton(antiguoPeaton);
        }
        else {
            //Inicio zona Critica de muerte de los peatones
            waitSemaforo(var.semMuertePeatones, 1, INFINITE);
            liberarPosicionPeaton(antiguoPeaton);
            liberarPosicionPeaton(actualPeaton);
            f.CRUCE_fin_peatOn();
            signalSemaforo(var.semCrearHilos, 1);
            signalSemaforo(var.semMuertePeatones, 1);
            //Fin de zona Critica de muerte de los peatones
            ExitThread(0);
        }
        
    }
}
DWORD WINAPI conduceCoche(LPVOID v)
{

    funciones *func;
    funciones f;
    func = (funciones *)v;
    f = *func;

    int culo = 0, pasajeros = 0, conductor = 0, morro = 0, distancia = 0, pedir = 0; //EL COCHE ESTA EN VERTICAL
    int culo_h = 0, pasajeros_h = 0, conductor_h = 0, morro_h = 0, distancia_h = 0;  //EL COCHE ESTA EN HORIZONTAL
    struct posiciOn pos1COCHE;
    int i;
    pos1COCHE = f.CRUCE_inicio_coche();
    if (pos1COCHE.y == 10)
    { //SALE DEL LADO --------------------------------------------------------------------------
        morro_h = 23;
        reservarPosicionCoche(morro_h);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        conductor_h = morro_h;
        morro_h++;
        reservarPosicionCoche(morro_h);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        pasajeros_h = conductor_h;
        conductor_h = morro_h;
        morro_h++;
        reservarPosicionCoche(morro_h);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        culo_h = pasajeros_h;
        pasajeros_h = conductor_h;
        conductor_h = morro_h;
        morro_h++;
        reservarPosicionCoche(morro_h);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        distancia_h = culo_h;
        culo_h = pasajeros_h;
        pasajeros_h = conductor_h;
        conductor_h = morro_h;
        morro_h++;
        reservarPosicionCoche(morro_h);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        //El coche ha salido completamente
        for (i = 25; i < 39; i++)
        {
            pedir = morro_h + 1;

            reservarPosicionCoche(pedir);
            pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
            f.CRUCE_pausa_coche();

            liberarPosicionCoche(distancia_h);
            distancia_h = culo_h;
            culo_h = pasajeros_h;
            pasajeros_h = conductor_h;
            conductor_h = morro_h;
            morro_h = pedir;
        }
        //Reserva las posiciones verticales
        distancia = 8;
        reservarPosicionCoche(distancia);
        culo = 9;
        reservarPosicionCoche(culo);
        pasajeros = 10;
        reservarPosicionCoche(pasajeros);
        conductor = 11;
        reservarPosicionCoche(conductor);
        morro = 12;
        reservarPosicionCoche(morro);
        //El coche está en vertical
        for (i = 11; i < 16; i++)
        {

            pedir = morro + 1;

            reservarPosicionCoche(pedir);
            pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
            f.CRUCE_pausa_coche();
            liberarPosicionCoche(distancia);
            distancia = culo;
            culo = pasajeros;
            pasajeros = conductor;
            conductor = morro;
            morro = pedir;
            if (i == 15) //Libera las posiciones horizontales que todavia tenia reservadas
            {
                liberarPosicionCoche(distancia_h);
                liberarPosicionCoche(culo_h);
                liberarPosicionCoche(pasajeros_h);
                liberarPosicionCoche(conductor_h);
                liberarPosicionCoche(morro_h);
            }
        }
        //El coche empieza a desaparecer
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(distancia);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(culo);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(pasajeros);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(conductor);
        liberarPosicionCoche(morro);
    }
    if (pos1COCHE.x == 33)
    { // SALE DE ARRIBA  --------------------------------------------------------------------------
        morro = 1;
        reservarPosicionCoche(morro);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        conductor = morro;
        morro++;
        reservarPosicionCoche(morro);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        pasajeros = conductor;
        conductor = morro;
        morro++;
        reservarPosicionCoche(morro);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        culo = pasajeros;
        pasajeros = conductor;
        conductor = morro;
        morro++;
        reservarPosicionCoche(morro);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        distancia = culo;
        culo = pasajeros;
        pasajeros = conductor;
        conductor = morro;
        morro++;
        reservarPosicionCoche(morro);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        //El coche ha salido completamente
        for (i = 5; i < 16; i++)
        {

            pedir = morro + 1;

            reservarPosicionCoche(pedir);
            pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
            f.CRUCE_pausa_coche();
            liberarPosicionCoche(distancia);
            distancia = culo;
            culo = pasajeros;
            pasajeros = conductor;
            conductor = morro;
            morro = pedir;
        }
        //El coche empieza a desaparecer
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(distancia);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(culo);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(pasajeros);
        pos1COCHE = f.CRUCE_avanzar_coche(pos1COCHE);
        f.CRUCE_pausa_coche();
        liberarPosicionCoche(conductor);
        liberarPosicionCoche(morro);
    }
    f.CRUCE_fin_coche();
    signalSemaforo(var.semCrearHilos, 1);
    ExitThread(0);
}

int reservarPosicionPeaton(struct posiciOn p)
{
    int pos = posSemaforo(p);
    waitSemaforo(var.tableroPeaton[pos], 1, INFINITE);
    return 0;
}
int liberarPosicionPeaton(struct posiciOn p)
{
    int pos = posSemaforo(p);
    signalSemaforo(var.tableroPeaton[pos], 1);
    return 0;
}
int reservarPosicionCoche(int pos)
{
    waitSemaforo(var.tableroCoche[pos], 1, INFINITE);
    return 0;
}
int liberarPosicionCoche(int pos)
{
    signalSemaforo(var.tableroCoche[pos], 1);
    return 0;
}
int semPeatonesVERDE(int semaforo)
{
    int i, j;
    if (semaforo == SEM_P1)
    {
        reservarPosicionCoche(14);
        reservarPosicionCoche(15); //Reserva para que los coches no atropellen a los peatones
        reservarPosicionCoche(16);
        for (i = 30; i <= 40; i++)
        {
            for (j = 13; j <= 15; j++)
            {
                struct posiciOn pos;
                pos.x = i;
                pos.y = j;
                liberarPosicionPeaton(pos);
            }
        }
        return 0;
    }
    else if (semaforo == SEM_P2)
    {
        for (i = 11; i >= 7; i--)
        {
            for (j = 27; j >= 21; j--)
            {
                struct posiciOn pos;
                pos.y = i;
                pos.x = j;
                liberarPosicionPeaton(pos);
            }
        }
        return 0;
    }
    return -1;
}
int semPeatonesROJO(int semaforo)
{
    int i, j;
    if (semaforo == SEM_P1)
    {
        for (i = 30; i <= 40; i++)
        {
            for (j = 13; j <= 15; j++)
            {
                struct posiciOn pos;
                pos.x = i;
                pos.y = j;
                reservarPosicionPeaton(pos);
            }
        }
        liberarPosicionCoche(14);
        liberarPosicionCoche(15); //Libera para que los coches ya puedan pasar
        liberarPosicionCoche(16);

        return 0;
    }
    else if (semaforo == SEM_P2)
    {
        for (i = 11; i >= 7; i--)
        {
            for (j = 27; j >= 21; j--)
            {
                struct posiciOn pos;
                pos.y = i;
                pos.x = j;
                reservarPosicionPeaton(pos);
            }
        }
        return 0;
    }
    return -1;
}
int crearTableroCoche(void)
{

    int i = 1;
    for (i = 1; i <= 17; i++) // Coche de arriba
    {

        var.tableroCoche[i] = CreateSemaphore(NULL, 1, 1, NULL);
        if (var.tableroCoche[i] == NULL)
        {
            return -1;
        }
    }
    for (i = 23; i <= 41; i++) // Coche del lado
    {

        var.tableroCoche[i] = CreateSemaphore(NULL, 1, 1, NULL);
        if (var.tableroCoche[i] == NULL)
        {
            return -1;
        }
    }
    return 0;
}
int accedeZonaCriticaCRUCE()
{
    int i;

    for (i = 32; i <= 40; i++) // ZONA CRITICA DEL HORIZONTAL
    {
        reservarPosicionCoche(i);
    }

    for (i = 7; i <= 11; i++) // ZONA CRITICA DEL VERTICAL
    {
        reservarPosicionCoche(i);
    }

    return 0;
}
int liberaZonaCriticaCRUCE()
{
    int i;
    for (i = 32; i <= 40; i++) // ZONA CRITICA DEL HORIZONTAL
    {
        liberarPosicionCoche(i);
    }
    for (i = 7; i <= 11; i++) // ZONA CRITICA DEL VERTICAL
    {
        liberarPosicionCoche(i);
    }
    return 0;
}