#include <stdio.h> //I/O
#include <unistd.h> //acces la POSIX-->procese, lucru cu fisiere
#include <stdlib.h> //functii generale-->ex:rand,malloc,free
#include <sys/wait.h> //control pentru procese
#include <sys/stat.h> //fstat
#include <sys/mman.h> //mmap,munmap
#include <fcntl.h> //lucru cu fisiere, file descriptors
#include <string.h> //lucru cu stringuri
#include <time.h> //setare de seed pentru swapuri



char *permutations;
char cuv[500000][30];
char *buff,*aux;

void swap(char msg[], int p1, int p2)
{char aux;
	aux=msg[p1];
	msg[p1]=msg[p2];
	msg[p2]=aux;
}

void encrypt(int poz, int fd)
{int p1,p2;
char op[10],text[50];
	if (strlen(cuv[poz])==1)
	{
		if(write(fd,"0-0 ",4)==-1)
			perror("write"),exit(1);
		return; //cuvant de lungime 1, nu are ce sa se schimbe
	}
	//printf("In encrypt at position %d\n",poz);
        srand(time(NULL));
        	
	for (int i=0;i<8;i++)
	{
		//alege 2 pozitii random din cuvant si permuta-le
		p1=rand()%(strlen(cuv[poz])-1);
		p2=rand()%(strlen(cuv[poz])-1);
		op[0]=p1+48;
		op[1]='-';
		op[2]=p2+48;
		op[3]=' ';

		swap(cuv[poz],p1,p2);
		//in fisierul pt permutari scrie interschimbarea folosita
		if(write(fd,op,4)==-1)
			perror("write"),exit(1);
			
	}

}

void start_encrypt(int fd, int perms, int min, int max)
{//printf("In start_encript with values %d, %d\n",min,max);
	
	for (int i=min;i<=max;i++)
	{	


		encrypt(i,perms);
		//dupa criptare, scrie cuvantul in fisierul pentru cuvinte criptate
		if (write(fd, cuv[i], strlen(cuv[i])) == -1) 
		{
            		perror("write to encrypted file");
            		exit(1);
        	}
        	
		 if (write(fd, " ", 1) == -1) 
		 {
            		perror("write space to encrypted file");
            		exit(1);
        	}

            	   if (write(perms, "\n", 1) == -1) {
            		perror("write newline to perms file");
            		exit(1);
        		}


	}
	
	
//printf("Exited encrypt\n");
}

char* decrypt(char *text, char *ops)
{int i=strlen(ops)-1,p1,p2;
	//printf("Decrypting the word %s\n",text);
	//printf("Using swaps %s\n",ops);
	//extragem pozitiile din permutari, 2 cate 2, de la coada la cap
	while (i>0)
	{
		p1=ops[i-1]-48;
		p2=ops[i-3]-48;
		swap(text,p1,p2);
		i-=4;
	}
	return(text);
}


int main(int argc, char *argv[], char *envp[])
{
	int pid,fd,n,ct=0,perms,dec,status,enc,is_space=0,l=0;
	char c,*space=" ",*end="\n";
	char *p;
	const char sep[5]=" \n";

	mode_t file_perms=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	struct stat file_stat,perms_stat;
	
	//sem_init(&sem,0,1);
	
	
	if (argc>3)
		printf("Usage: ./encryptor_body <filename> to encrypt text or ./encryptor_body <encrypted_file> <perms_used>to decrypt\n"),exit(0);
	else if (argc==1)
	{
		printf("Hello. This application can be used to encrypt a textfile, or decrypt an encrypted file\n");
		printf("\nTo encrypt: add only ONE textfile parameter.\nYou can find your encrypted file in this folder, having the name:<your_file>_encrypted\n");
		printf("You will also receive a file called 'perms' which stores how your words have been encrypted");
		printf("\nTo decrypt: add TWO textfile parameters: One being the encrypted file and the other being the file used to store how your words were encrypted.\nYou can find your decrypted file in this folder, having the name:<your_file>_decrypted");
		
	}
	//ENCRYPT
	else if (argc==2)
	{

	
	//cream sau dam truncate fisierului care pastreaza permutarile folosite
	if ((perms=open("perms",O_RDWR|O_CREAT|O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))<0)
		perror("open"),exit(1);
		
	//deschide fisierul de criptat
	if((fd=open(argv[1],O_RDWR))<0)
		perror("open"),exit(1);
		
        if (fstat(fd, &file_stat) == -1)
            perror("fstat"), exit(1);
	
		
	buff = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (buff == MAP_FAILED)
            perror("mmap"), exit(1);
        
        ct=0;
	
	//separam cuvintele si le retinem intr-un vector de cuvinte
	
        for (int i = 0; i < file_stat.st_size; ++i)
        {
        	if (buff[i]!=' ' && is_space==1)
        	{
        		ct++;
        		if (ct>=500000)
        			break;
        		l=0;
        		cuv[ct][l]=buff[i];
        		is_space=0;
        		if (l<30)
        			l++;
        		
        	}
        	else if (buff[i]!=' ' && is_space==0)
        	{
        		cuv[ct][l]=buff[i];
        		l++;
        	}
        	else if (buff[i]==' ')
        		is_space=1;

	}
       
	
	//inchidem fisierul initial, nu va mai fi folosit in viitor
	close(fd);
	munmap(buff, file_stat.st_size);
	//cream(sau truncam) fisierul care va contine cuvintele create
	if ((fd=open(strcat(argv[1],"_encrypted"),O_RDWR|O_CREAT|O_TRUNC,file_perms))<0)
		perror("open"),exit(1);
		

	//creeaza 4 procese diferite, care se ocupa de a cripta un sfert din cuvinte fiecare
	int proc=1;
	printf("Total word count: %d\n",ct);
	pid=fork();
	if (pid<0)
		perror("fork"),exit(1);
	
	else if (pid==0)
	{
		//PROCESUL 1
		//printf("%d\n",ct);

		int new_fd=dup(fd);
		int new_perms=dup(perms);
		close(fd);
		close(perms);	
		//printf("This is child process %d\n",proc);
		if (ct!=1)
			start_encrypt(new_fd,new_perms,0,ct/4);
		else
			start_encrypt(new_fd,new_perms,0,1);
		close(new_fd);
		close(new_perms);


	
	}
	else
	{
		waitpid(pid, &status, 0);
		//printf("Exited process %d\n",proc);
		proc++;
		pid=fork();
		if (pid<0)
			perror("fork"),exit(1);
		else if (pid==0)
		{

			//PROCESUL 2

			int new_fd=dup(fd);
			int new_perms=dup(perms);
			close(fd);
			close(perms);	
			//printf("This is child process %d\n",proc);
			if (ct>2 && ct!=1)
				start_encrypt(new_fd,new_perms,ct/4+1,ct/2);
			else if (ct==2)
				start_encrypt(new_fd,new_perms,0,2);
				close(new_fd);


		}
		else
		{
			waitpid(pid, &status, 0);
			//printf("Exited process %d\n",proc);
			proc++;
			pid=fork();
			if (pid<0)
				perror("fork"),exit(1);
			else if (pid==0)
			{
				//PROCESUL 3

				int new_fd=dup(fd);
				int new_perms=dup(perms);	
				close(fd);
				close(perms);			
				//printf("This is child process %d\n",proc);
				if (ct>3)
					start_encrypt(new_fd,new_perms,ct/2+1,ct*3/4);
				else if (ct==3)
					start_encrypt(new_fd,new_perms,0,3);
						close(new_fd);
				close(new_perms);


			}
			else
			{
				waitpid(pid, &status, 0);
				//printf("Exited process %d\n",proc);
				proc++;
				pid=fork();
				if (pid<0)
					perror("fork"),exit(1);
				else if (pid==0)
				{
					//PROCESUL 4

					int new_fd=dup(fd);
					int new_perms=dup(perms);
					close(fd);
					close(perms);	
					//printf("This is child process %d\n",proc);
					if (ct>4 && ct!=1 && ct!=2 && ct!=3)
						start_encrypt(new_fd,new_perms,ct*3/4+1,ct-1);
					else if (ct==4)
						start_encrypt(new_fd,new_perms,0,4);
					close(new_fd);
					close(new_perms);


				}
				else
				{
					waitpid(pid, &status, 0);
					//printf("Exited process %d\n",proc);

				}
			}
		}
	}

	//inchidem file descriptors
	close(fd);
	close(perms);

	}

	///DECRYPT
	if (argc==3)
	{
		if (strcmp(argv[2],"perms")!=0)
			printf("Error: 2nd parameter is not the perms file!"),exit(1);
		//deschidem fisierul cu cuv criptate si cel cu permutarile utilizate
		if((fd=open(argv[1],O_RDWR))<0)
			perror("open"),exit(1);
		
		if ((perms=open(argv[2],O_RDWR))<0)
			perror("open"),exit(1);
		
		if (fstat(fd, &file_stat) == -1)
            	perror("fstat"), exit(1);
		
		buff = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        	if (buff == MAP_FAILED)
            		perror("mmap"), exit(1);
            	

		
		//ca la criptare, retinem in vectorul de cuvinte
		
		for (int i = 0; i < file_stat.st_size; ++i)
		{
			if (buff[i]!=' ' && is_space==1)
			{
				ct++;
				if (ct>=500000)
        				break;
				l=0;
				cuv[ct][l]=buff[i];
				is_space=0;
				if (l<30)
        				l++;
			}
			else if (buff[i]!=' ' && is_space==0)
			{
				cuv[ct][l]=buff[i];
				l++;
			}
			else if (buff[i]==' ')
				is_space=1;

		}
        
		munmap(buff, file_stat.st_size);
		if (fstat(perms, &perms_stat) == -1)
            		perror("fstat"), exit(1);
		
		
		permutations = mmap(NULL, perms_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, perms, 0);
        	if (permutations == MAP_FAILED)
            		perror("mmap"), exit(1);
            	
		
		//cream(sau truncam) fisierul care va contine cuvintele decriptate
		if ((dec=open(strcat(argv[1],"_decrypted"),O_RDWR|O_CREAT|O_TRUNC,file_perms))<0)
			perror("open"),exit(1);
		
		//citim permutarile folosite

        	ct=0;
        	//in practica, fisierul va fi decriptat complet dupa prima utilizare, deci perms va fi inutil dupa
		p=strtok(permutations,"\n");
		while (p!=NULL)
		{

			write(dec,decrypt(cuv[ct],p),strlen(cuv[ct]));
			write(dec,space,1);
			p=strtok(NULL,"\n");
			ct++;
		}
		//daca se doreste stergerea fisierului
		//unlink("perms");
		munmap(permutations, perms_stat.st_size);
	}


}
		

