#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "cruce.h"

//DECLARACIÓN DE VARIABLES GLOBALES
struct globales
{
	pid_t padre;
	int semaforos;
	int mComp;
	int buzon;
	char *zona;
} global;

typedef struct mensaje
{
	long tipo;
} mensaje;

//********PARA ENCINA HAY QUE DESCOMENTAR ESTO********

union semun
{
	int val;
	struct semid_ds *buf;
	ushort_t *array;
};

//****************************************************

//Prototipos de las funciones utilizadas en el programa:
//SEMAFOROS PROCESOS:
int crearSemaforo(int numero);
int eliminaSemaforo(int semaforo);
int semSignal(int semaforo, int cuanto, int objetivo);
int semWait(int semaforo, int cuanto, int objetivo);

//MEMORIA COMPARTIDA:
int crearMemoriaCompartida(int tam);
char *convertirMCompartidaChar(int memoriaCompartida);
int liberarMemoriaCompartida(int memoriaCompartida);

//BUZONES:
int crearBuzon(void);
int liberarBuzon(int buzon);
int recibirMensajeBlo(long tipo, int buzon);
int crearMensaje_enviarMensaje(long tipo, int buzon);
int enviarMensaje(mensaje msg, int buzon);
int plagaTodoMensajes(int buzon);
int plagaTodoMensajesPeaton(int buzon);

//ZONA CRITICA COCHE:
int liberaZonaCritica();
int accedeZonaCritica();

//SALIR DEL PROGRAMA:
void noZombies();
void no_nada() {}
void configuraAborto();
void abortaPrograma();

//CONTROLAR CRUCE
int cicloSemaforico();
int conduceCoche();

//PEATONES:
int andaPeaton();
long posBuzonPeatones(struct posiciOn pos);
int semPeatonesVERDE(int semaforo, int buzon);
int semPeatonesROJO(int semaforo, int buzon);

int main(int argc, char const *argv[])
{
	// COMPROBACION INICIAL - PASO POR LINEA DE ORDENES -
	int velocidad, nProcesos;
	if (argc != 3)
	{
		printf("ERROR!! : Argumentos pasados incorrectamente\n");
		printf("Se deben pasar 2 enteros (la velocidad y el num max de procesos)\n\n");
		return (-1);
	}
	else
	{
		nProcesos = atoi(argv[1]);
		if (nProcesos <= 3 || nProcesos > 49)
		{
			printf("EL NUMERO DE PROCESOS ES INCORRECTO!!\n");
			return -1;
		}
		velocidad = atoi(argv[2]);
		if (velocidad < 0)
		{
			printf("ERROR LA VELOCIDAD TIENE QUE SER UN ENTERO POSITIVO!!\n");
			return -1;
		}
	}

	configuraAborto(); //PREPARA LA SALIDA DEL PROGRAMA CON CTRL + C (PARA QUE SALTE A LA MANEJADORA)

	global.padre = getpid();

	//Declaración de los semaforos utilizados
	global.semaforos = crearSemaforo(3);

	//Declaración de la memoria compartida
	global.mComp = crearMemoriaCompartida(1024);
	global.zona = convertirMCompartidaChar(global.mComp);

	//Declaración de la memoria compartida
	global.buzon = crearBuzon();

	if (plagaTodoMensajes(global.buzon) != 0)
	{
		perror("ERROR PLAGA Coche");
		abortaPrograma();
	}
	if (plagaTodoMensajesPeaton(global.buzon) != 0)
	{
		perror("ERROR PLAGA Peaton");
		abortaPrograma();
	}
	
	//-------------SEMAFORO 1-------------
	//Descomentar para encina:
	union semun semValor1;
	semValor1.val=nProcesos-3;
	//Descomentar para local
	//int semValor1 = nProcesos - 3;

	if (semctl(global.semaforos, 1, SETVAL, semValor1) == -1)
	{
		perror("ERROR EN LOS SEMAFOROS!!");
		abortaPrograma();
	};
	//-------------SEMAFORO 2-------------
	//Descomentar para encina:
	union semun semValor2;
	semValor2.val=1;
	//Descomentar para local
	//int semValor2 = 1;

	if (semctl(global.semaforos, 2, SETVAL, semValor2) == -1)
	{
		perror("ERROR EN LOS SEMAFOROS!!");
		abortaPrograma();
	};

	CRUCE_inicio(velocidad, nProcesos, global.semaforos, global.zona);
	int coche_peaton;
	int vFork;
	vFork = fork();
	{
		if (vFork != 0) //PADRE
		{
			while (1)
			{
				//Este semaforo controla la creación de los procesos
				//y si no puede crear más se queda esperando
				semWait(global.semaforos, 1, 1);
				coche_peaton = CRUCE_nuevo_proceso();

				int vFork2 = fork();
				{
					if (vFork2 == 0) //HIJO
					{

						if (coche_peaton == COCHE)
						{
							if (conduceCoche() == -1)
							{
								perror("ERROR EN LA CONDUCCION");
								abortaPrograma();
							}
							semSignal(global.semaforos, 1, 1);
							exit(0);
						}
						else if (coche_peaton == PEAToN)
						{
							if (andaPeaton() == -1)
							{
								perror("ERROR ANDANDO");
								abortaPrograma();
							}
							semSignal(global.semaforos, 1, 1);
							exit(0);
						}
					}
					if (vFork2 == -1)
					{
						perror("Error al crear vFork!!");
						exit(-1);
					}
				}
			}
		}
		if (vFork == 0) //HIJO QUE SE ENCARGA DEL CICLO SEMAFORICO
		{
			if(cicloSemaforico()==-1){
				perror("Error ciclo semaforico");
				abortaPrograma();
			}
		}
		if (vFork == -1)
		{
			perror("Error al crear vFork!!");
			exit(-1);
		}
	}
	abortaPrograma();
}

//------FUNCIONES UTILIZADAS------//

//*****SEMAFOROS*****//
int crearSemaforo(int numero)
{
	int semaforo;
	if ((semaforo = semget(IPC_PRIVATE, numero, IPC_CREAT | 0600)) == -1)
	{
		perror("Error en creación de semaforo");
		return -1;
	}
	return semaforo;
}
int eliminaSemaforo(int semaforo)
{
	int sem;
	if (sem = (semctl(semaforo, 0, IPC_RMID) == -1))
	{
		perror("Error en eliminacion de semaforo");
		return -1;
	}
	return sem;
}
int semSignal(int semaforo, int cuanto, int objetivo)
{
	struct sembuf uso[] = {objetivo, cuanto, 0};
	int sem;
	if (sem = semop(semaforo, uso, 1) == -1)
	{
		perror("Error en Signal de semaforo");
		return -1;
	}
	return sem;
}
int semWait(int semaforo, int cuanto, int objetivo)
{
	struct sembuf uso[] = {objetivo, -cuanto, 0};
	int sem;
	if (sem = semop(semaforo, uso, 1) == -1)
	{
		perror("Error en Signal de semaforo");
		return -1;
	}
	return sem;
}

//*****BUZONES*****//
int crearBuzon(void)
{
	int buzon;
	if ((buzon = msgget(IPC_PRIVATE, IPC_CREAT | 0777)) == -1)
	{
		perror("Error en creación de buzon");
		return -1;
	}
	return buzon;
}
int liberarBuzon(int buzon)
{
	int buz;
	if (buz = (msgctl(buzon, IPC_RMID, NULL)) == -1)
	{
		perror("Error al eliminar buzon!!");
		return -1;
	}
	return buz;
}
int crearMensaje_enviarMensaje(long tipo, int buzon)
{
	mensaje msg;
	msg.tipo = tipo;
	if (enviarMensaje(msg, buzon) == -1)
	{
		perror("MAL CREADO MENSAJE -1\n");
		return -1;
	}
	return 0;
}
int enviarMensaje(mensaje msg, int buzon)
{
	int error;
	error = msgsnd(buzon, &msg, sizeof(msg) - sizeof(long), 0);
	return error;
}
int plagaTodoMensajes(int buzon)
{
	int i = 1;
	for (i = 1; i <= 17; i++) // RECORRIDO DEL COCHE DE ARRIBA
	{
		if (i != 6) //LOS CREA EL CICLO SEMAFORICO
		{
			crearMensaje_enviarMensaje(i, buzon);
		}
	}
	for (i = 23; i <= 41; i++) // RECORRIDO DEL COCHE DEL LADO
	{
		if (i != 31) //LOS CREA EL CICLO SEMAFORICO
		{
			crearMensaje_enviarMensaje(i, buzon);
		}
	}
	return 0;
}
int plagaTodoMensajesPeaton(int buzon)
{
	int i, j;
	int nMensaje;
	for (i = 0; i <= 6; i++) //ZONA CRITICA 1 -- Campo de arriba a la izquierda
	{
		for (j = 0; j <= 29; j++)
		{
			nMensaje = 100 + i * 51 + j;
			if (crearMensaje_enviarMensaje(nMensaje, buzon) == -1)
			{
				perror("error crear zona critica peatones 1");
				return -1;
			}
		}
	}
	for (i = 12; i <= 16; i++) //ZONA CRITICA 2 -- Campo de abajo a la izquierda
	{
		for (j = 0; j <= 29; j++)
		{
			nMensaje = 100 + i * 51 + j;
			if (crearMensaje_enviarMensaje(nMensaje, buzon) == -1)
			{
				perror("error crear zona critica peatones 2");
				return -1;
			}
		}
	}
	for (i = 0; i <= 16; i++) //ZONA CRITICA 3 -- Campo de la derecha
	{
		for (j = 41; j <= 50; j++)
		{
			nMensaje = 100 + i * 51 + j;
			if (crearMensaje_enviarMensaje(nMensaje, buzon) == -1)
			{
				perror("error crear zona critica peatones 3");
				return -1;
			}
		}
	}
	for (i = 11; i >= 7; i--) //ZONA CRITICA 4 -- SEM_P2 -- HORIZONTAL
	{
		for (j = 27; j >= 21; j--)
		{
			nMensaje = 100 + i * 51 + j;

			if (crearMensaje_enviarMensaje(nMensaje, buzon) == -1)
			{
				perror("error crear zona critica peatones 4");
				return -1;
			}
		}
	}
	for (i = 30; i <= 40; i++) //ZONA CRITICA 5 -- SEM_P1 -- VERTICAL
	{
		for (j = 13; j <= 15; j++)
		{
			nMensaje = 100 + j * 51 + i;
			if (crearMensaje_enviarMensaje(nMensaje, buzon) == -1)
			{
				perror("error crear zona critica peatones 5");
				return -1;
			}
		}
	}

	return 0;
}
int recibirMensajeBlo(long tipo, int buzon)
{
	mensaje msg;
	int error;
	error = msgrcv(buzon, &msg, sizeof(msg) - sizeof(long), tipo, 0);

	if (error == -1)
	{
		perror("ERROR MENSAJE MAL RECIBIDO");
		return error;
	}
	return error;
}

//*****MEMORIA COMPARTIDA*****//
int crearMemoriaCompartida(int tam)
{
	return shmget(IPC_PRIVATE, tam, IPC_CREAT | 0777);
}
char *convertirMCompartidaChar(int memoriaCompartida)
{
	return (char *)shmat(memoriaCompartida, 0, 0);
}
int liberarMemoriaCompartida(int memoriaCompartida)
{
	return shmctl(memoriaCompartida, IPC_RMID, NULL);
}

//*****FUNCIONES CRUCE*****
int cicloSemaforico()
{
	int i = 0;
	long posSemaforo_C1 = 6, posSemaforo_C2 = 31;	 //SON LAS POSICIONES ANTERIORES A LOS SEMAFOROS

	//-----------------------Inicializaciones previas----------------------//
	if (semPeatonesROJO(SEM_P2, global.buzon) == -1) //Crea los mensajes correspondientes al SEM_P2
	{
		perror("Semaforo mal cambiado");
		return -1;
	}
	recibirMensajeBlo(14, global.buzon);
	recibirMensajeBlo(15, global.buzon); //Reservamos los mensajes correspondonetes a SEM_P1 para que los coches
	recibirMensajeBlo(16, global.buzon); //no se lo salten luego
	while (1)
	{
		//FASE 1:
		CRUCE_pon_semAforo(SEM_C2, ROJO);
		//--------------------------------------------//
		if (semPeatonesROJO(SEM_P1, global.buzon) == -1)
		{
			perror("Semaforo mal cambiado");
			return -1;
		}
		CRUCE_pon_semAforo(SEM_P1, ROJO);
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_C1, VERDE);
		if(crearMensaje_enviarMensaje(posSemaforo_C1, global.buzon)==-1){
			perror("Mensaje mal creado");
			return -1;
		}
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_P2, VERDE);
		if (semPeatonesVERDE(SEM_P2, global.buzon) == -1)
		{
			perror("Semaforo mal cambiado");
			return -1;
		}
		//--------------------------------------------//
		for (i = 0; i < 6; i++)
		{
			pausa();
		}
		//------------------AMARILLO---------------------//
		if (recibirMensajeBlo(posSemaforo_C1, global.buzon) == -1)
		{
			perror("Mensaje mal recibido");
			return -1;
		}
		CRUCE_pon_semAforo(SEM_C1, AMARILLO);
		for (i = 0; i < 2; i++)
		{
			pausa();
		}
		//COMPROBAR DE LA 32 a la 40 (ZONA CRITICA DEL CRUCE)
		//Y TAMBIEN DE LA 7 a la 11
		accedeZonaCritica();
		liberaZonaCritica();
		//------------------FIN AMARILLO-----------------//

		//FASE 2:
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_C1, ROJO);
		//--------------------------------------------//
		if (semPeatonesROJO(SEM_P2, global.buzon) == -1)
		{
			perror("Semaforo mal cambiado");
			return -1;
		}
		CRUCE_pon_semAforo(SEM_P2, ROJO);
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_C2, VERDE);
		if (crearMensaje_enviarMensaje(posSemaforo_C2, global.buzon) == -1)
		{
			perror("Mensaje mal creado");
			return -1;
		}
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_P1, ROJO);
		//--------------------------------------------//
		for (i = 0; i < 9; i++)
		{
			pausa();
		}
		//------------------AMARILLO---------------------//
		if (recibirMensajeBlo(posSemaforo_C2, global.buzon) == -1)
		{
			perror("Mensaje mal recibido");
			return -1;
		}
		CRUCE_pon_semAforo(SEM_C2, AMARILLO);
		for (i = 0; i < 2; i++)
		{
			pausa();
		}
		//COMPROBAR DE LA 32 a la 40 (ZONA CRITICA DEL CRUCE)
		//Y TAMBIEN DE LA 7 a la 11
		accedeZonaCritica();
		liberaZonaCritica();
		//------------------FIN AMARILLO-----------------//

		//FASE 3
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_C1, ROJO);
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_C2, ROJO);
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_P1, VERDE);
		if (semPeatonesVERDE(SEM_P1, global.buzon) == -1)
		{
			perror("Semaforo mal cambiado");
			return -1;
		}
		//--------------------------------------------//
		CRUCE_pon_semAforo(SEM_P2, ROJO);
		//--------------------------------------------//
		for (i = 0; i < 12; i++)
		{
			pausa();
		}
	}
}
int conduceCoche()
{
	long culo = 0, pasajeros = 0, conductor = 0, morro = 0, distancia = 0, pedir = 0; //EL COCHE ESTA EN VERTICAL
	long culo_h = 0, pasajeros_h = 0, conductor_h = 0, morro_h = 0, distancia_h = 0;  //EL COCHE ESTA EN HORIZONTAL
	int i;
	struct posiciOn pos1COCHE;
	pos1COCHE = CRUCE_inicio_coche();
	if (pos1COCHE.y == 10) //SALE DEL LADO --------------------------------------------------------------------
	{

		morro_h = 23;
		if (recibirMensajeBlo(morro_h, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		conductor_h = morro_h;
		morro_h++;
		if (recibirMensajeBlo(morro_h, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		pasajeros_h = conductor_h;
		conductor_h = morro_h;
		morro_h++;
		if (recibirMensajeBlo(morro_h, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		culo_h = pasajeros_h;
		pasajeros_h = conductor_h;
		conductor_h = morro_h;
		morro_h++;
		if (recibirMensajeBlo(morro_h, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		distancia_h = culo_h;
		culo_h = pasajeros_h;
		pasajeros_h = conductor_h;
		conductor_h = morro_h;
		morro_h++;
		if (recibirMensajeBlo(morro_h, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		//HA APARECIDO EL COCHE ENTERO
		for (i = 25; i < 39; i++)
		{
			pedir = morro_h + 1;

			if (recibirMensajeBlo(pedir, global.buzon) == -1)
			{
				return -1;
			}
			pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
			pausa_coche();

			if (crearMensaje_enviarMensaje(distancia_h, global.buzon) == -1)
			{
				return -1;
			}
			distancia_h = culo_h;
			culo_h = pasajeros_h;
			pasajeros_h = conductor_h;
			conductor_h = morro_h;
			morro_h = pedir;
		}

		//RESERVA LA ZONA DE GIRO
		distancia = 8;
		if (recibirMensajeBlo(distancia, global.buzon) == -1)
		{
			return -1;
		}

		culo = 9;
		if (recibirMensajeBlo(culo, global.buzon) == -1)
		{
			return -1;
		}

		pasajeros = 10;
		if (recibirMensajeBlo(pasajeros, global.buzon) == -1)
		{
			return -1;
		}

		conductor = 11;
		if (recibirMensajeBlo(conductor, global.buzon) == -1)
		{
			return -1;
		}

		morro = 12;
		if (recibirMensajeBlo(morro, global.buzon) == -1)
		{
			return -1;
		}

		//EL COCHE ESTÁ EN VERTICAL
		for (i = 11; i < 16; i++)
		{

			pedir = morro + 1;

			if (recibirMensajeBlo(pedir, global.buzon) == -1)
			{
				return -1;
			}
			pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
			pausa_coche();
			if (crearMensaje_enviarMensaje(distancia, global.buzon) == -1)
			{
				return -1;
			}
			distancia = culo;
			culo = pasajeros;
			pasajeros = conductor;
			conductor = morro;
			morro = pedir;
			if (i == 15) //YA HA HECHO EL GIRO LIBERA LAS HORIZONTALES
			{

				if (crearMensaje_enviarMensaje(distancia_h, global.buzon) == -1)
				{
					return -1;
				}
				if (crearMensaje_enviarMensaje(culo_h, global.buzon) == -1)
				{
					return -1;
				}
				if (crearMensaje_enviarMensaje(pasajeros_h, global.buzon) == -1)
				{
					return -1;
				}
				if (crearMensaje_enviarMensaje(conductor_h, global.buzon) == -1)
				{
					return -1;
				}
				if (crearMensaje_enviarMensaje(morro_h, global.buzon) == -1)
				{
					return -1;
				}
			}
		}
		//EL COCHE EMPIEZA A SALIR DE LA PANTALLA
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(distancia, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(culo, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(pasajeros, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(conductor, global.buzon) == -1)
		{
			return -1;
		}
		if (crearMensaje_enviarMensaje(morro, global.buzon) == -1)
		{
			return -1;
		}
	}
	if (pos1COCHE.x == 33) //SALE DE ARRIBA --------------------------------------------------------------------
	{

		morro = 1;
		if (recibirMensajeBlo(morro, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		conductor = morro;
		morro++;
		if (recibirMensajeBlo(morro, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		pasajeros = conductor;
		conductor = morro;
		morro++;
		if (recibirMensajeBlo(morro, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		culo = pasajeros;
		pasajeros = conductor;
		conductor = morro;
		morro++;
		if (recibirMensajeBlo(morro, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		distancia = culo;
		culo = pasajeros;
		pasajeros = conductor;
		conductor = morro;
		morro++;
		if (recibirMensajeBlo(morro, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		//EL COCHE HA APARECIDO COMPLETAMENTE

		for (i = 5; i < 16; i++)
		{

			pedir = morro + 1;

			if (recibirMensajeBlo(pedir, global.buzon) == -1)
			{
				return -1;
			}
			pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
			pausa_coche();
			if (crearMensaje_enviarMensaje(distancia, global.buzon) == -1)
			{
				return -1;
			}
			distancia = culo;
			culo = pasajeros;
			pasajeros = conductor;
			conductor = morro;
			morro = pedir;
		}
		//EL COCHE EMPIEZA A DESAPARECER
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(distancia, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(culo, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(pasajeros, global.buzon) == -1)
		{
			return -1;
		}
		pos1COCHE = CRUCE_avanzar_coche(pos1COCHE);
		pausa_coche();
		if (crearMensaje_enviarMensaje(conductor, global.buzon) == -1)
		{
			return -1;
		}
		if (crearMensaje_enviarMensaje(morro, global.buzon) == -1)
		{
			return -1;
		}
	}

	CRUCE_fin_coche();
	return 1;
}

//*****FINALIZAR PROGRAMA*****
void configuraAborto()
{
	sigset_t mask1;
	struct sigaction salida;
	salida.sa_handler = abortaPrograma;
	salida.sa_mask = mask1;
	salida.sa_flags = 0;
	sigfillset(&mask1);
	sigdelset(&mask1, SIGINT);
	if (-1 == sigaction(SIGINT, &salida, NULL))
	{
		perror("ERR: sigaction salida");
		exit(-1);
	}
}
void noZombies()
{
	sigset_t mask1;
	struct sigaction salida;
	salida.sa_handler = no_nada;
	salida.sa_mask = mask1;
	salida.sa_flags = 0;
	sigfillset(&mask1);
	sigdelset(&mask1, SIGCHLD);
	if (-1 == sigaction(SIGCHLD, &salida, NULL))
	{
		perror("ERR: sigaction salida");
		exit(-1);
	}
}

void abortaPrograma()
{
	if (getpid() != global.padre)
	{
		exit(0);
	}
	noZombies();
	if (eliminaSemaforo(global.semaforos) != 0)
	{
		perror("Semaforos mal eliminados!!");
		exit(-1);
	}
	if (liberarBuzon(global.buzon) != 0)
	{
		perror("Buzon mal eliminado!!");
		exit(-1);
	}
	CRUCE_fin();
	if (liberarMemoriaCompartida(global.mComp) != 0)
	{
		perror("M.Compartida mal eliminada!!");
		exit(-1);
	}
	exit(0);
}

//*****ZONA CRITICA*****
int accedeZonaCritica()
{
	int i;

	for (i = 32; i <= 40; i++) // ZONA CRITICA DEL HORIZONTAL
	{
		if (recibirMensajeBlo(i, global.buzon) == -1)
		{
			return -1;
		}
	}

	for (i = 7; i <= 11; i++) // ZONA CRITICA DEL VERTICAL
	{

		if (recibirMensajeBlo(i, global.buzon) == -1)
		{
			return -1;
		}
	}

	return 0;
}
int liberaZonaCritica()
{
	int i;
	for (i = 32; i <= 40; i++) // ZONA CRITICA DEL HORIZONTAL
	{
		if (crearMensaje_enviarMensaje(i, global.buzon) == -1)
		{
			return -1;
		}
	}
	for (i = 7; i <= 11; i++) // ZONA CRITICA DEL VERTICAL
	{
		if (crearMensaje_enviarMensaje(i, global.buzon) == -1)
		{
			return -1;
		}
	}
	return 0;
}

//*****PEATONES*****
int andaPeaton()
{
	char a[50];
	struct posiciOn antiguoPeaton;
	semWait(global.semaforos, 1, 2);
	struct posiciOn actualPeaton;
	
	struct posiciOn sigPeaton = CRUCE_inicio_peatOn_ext(&actualPeaton);
	recibirMensajeBlo(posBuzonPeatones(actualPeaton), global.buzon);
	semSignal(global.semaforos, 1, 2); 

	while (sigPeaton.y >= 0)
	{
		
		recibirMensajeBlo(posBuzonPeatones(sigPeaton), global.buzon);
		antiguoPeaton = actualPeaton;
		actualPeaton = sigPeaton;
		sigPeaton = CRUCE_avanzar_peatOn(sigPeaton);
		
		pausa();

		crearMensaje_enviarMensaje(posBuzonPeatones(antiguoPeaton), global.buzon);
	}
	crearMensaje_enviarMensaje(posBuzonPeatones(actualPeaton), global.buzon);

	CRUCE_fin_peatOn();

	return 0;
}
int semPeatonesVERDE(int semaforo, int buzon)
{
	int i, j, nMensaje;
	if (semaforo == SEM_P1)
	{
		if(recibirMensajeBlo(14, buzon)==-1){
			perror("Mensaje mal recibido");
			return -1;
		}
		if (recibirMensajeBlo(15, buzon) == -1) //PARA QUE NO ATROPELLE EL COCHE A LOS PEATONES
		{
			perror("Mensaje mal recibido");
			return -1;
		}
		if (recibirMensajeBlo(16, buzon) == -1)
		{
			perror("Mensaje mal recibido");
			return -1;
		}
		for (i = 30; i <= 40; i++) //ZONA CRITICA 5 -- SEM_P1 -- VERTICAL
		{
			for (j = 13; j <= 15; j++)
			{
				nMensaje = 100 + j * 51 + i;
				if (crearMensaje_enviarMensaje(nMensaje, buzon) == -1)
				{
					perror("error enviar mensaje");
					return -1;
				}
			}
		}
		return (0);
	}
	if (semaforo == SEM_P2)
	{
		for (i = 11; i >= 7; i--) //ZONA CRITICA 4 -- SEM_P2 -- HORIZONTAL
		{
			for (j = 27; j >= 21; j--)
			{
				nMensaje = 100 + i * 51 + j;

				if (crearMensaje_enviarMensaje(nMensaje, buzon) == -1)
				{
					perror("error enviar mensaje");
					return -1;
				}
			}
		}
		return 0;
	}
	return -1;
}
int semPeatonesROJO(int semaforo, int buzon)
{
	int i, j, nMensaje;
	if (semaforo == SEM_P1)
	{

		for (i = 30; i <= 40; i++) //ZONA CRITICA 5 -- SEM_P1 -- VERTICAL
		{
			for (j = 13; j <= 15; j++)
			{
				nMensaje = 100 + j * 51 + i;
				if (recibirMensajeBlo(nMensaje, buzon) == -1)
				{
					perror("error recibir mensajes");
					return -1;
				}
			}
		}
		if (crearMensaje_enviarMensaje(14, buzon) == -1)
		{
			perror("error enviar mensaje");
			return -1;
		}
		if (crearMensaje_enviarMensaje(15, buzon) == -1) //PARA QUE NO ATROPELLE EL COCHE A LOS PEATONES
		{
			perror("error enviar mensaje");
			return -1;
		}
		if (crearMensaje_enviarMensaje(16, buzon) == -1)
		{
			perror("error enviar mensaje");
			return -1;
		}
		return (0);
	}
	if (semaforo == SEM_P2)
	{
		for (i = 11; i >= 7; i--) //ZONA CRITICA 4 -- SEM_P2 -- HORIZONTAL
		{
			for (j = 27; j >= 21; j--)
			{
				nMensaje = 100 + i * 51 + j;

				if (recibirMensajeBlo(nMensaje, buzon) == -1)
				{
					perror("error recibir mensajes");
					return -1;
				}
			}
		}
		return 0;
	}
	return -1;
}
long posBuzonPeatones(struct posiciOn pos)
{
	if (pos.x >= 0 && pos.x <= 50 && pos.y >= 0 && pos.y <= 16)
	{
		return (100 + pos.y * 51 + pos.x);
	}
	else
		return (-1);
}
