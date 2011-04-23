/**
*
* @author (c) Copyright 2006, Peter Jakub�o <pjakubco@gmail.com>
*
* @section Uvod �vod
* Program sl��i na defragmentovanie obrazu s�borov�ho syst�mu FAT32. Je rozdelen�
* do niekolk�ch funk�n�ch modulov, z ktor�ch ka�d� dok�e pracova� so svojou
* probl�movou dom�nou. Jednotliv� moduly navz�jom spolupracuj� volan�m funkci�,
* ktor� s� viditeln� (teda ktor�ch prototyp je deklarovan� v hlavi�kov�ch s�boroch).
* Mojou snahou bolo zviditelni� �o mo�no najmen�� po�et funkci�, aby som dosiahol �o
* mo�no najv��iu "zap�zdrenos�" (aj ke� C nie je objektov�). Umo�nilo mi to potom
* lah�ie odchytenie a odladenie ch�b.
* 
* Pri p�san� tohto projektu som mal na mysli motto: Keep It Simple Stupid (KISS).
* V pr�pade nejak�ch ot�zok, nap�te mi na mail pjakubco@gmail.com
*
* <hr>
* @section Kompilovanie Kompilovanie programu
* Pre ulah�enie kompilovania som pou�il program make a vytvoril som s�bor s pravidlami Makefile. Programy
* s� nap�san� v�lu�ne pre kompil�tor gcc a na to, aby sa dal projekt skompilova�, je potrebn� ma� nain�talovan�
* program sed a xgettext.
* 
* Pravidl� v Makefile s� navrhnut� tak, aby sa spravila kompil�cia ka�d�ho s�boru, ktor� m� pr�ponu .cpp, �i�e
* pravidl� nez�visia od zdrojov�ch s�borov. Okrem toho sa pre ka�d� zdrojov� s�bor zistia (pomocou gcc s prep�na�om
* -MM) z�vislosti na hlavi�kov�ch s�boroch, tieto z�vislosti sa zap�u do samostatn�ho s�boru s pr�ponou .d, ktor� sa
* pomocou make funkcie -include vlo�� do Makefile a je tak zabezpe�en� aj kontrola �asovej zmeny hlavi�kov�ch s�borov,
* a je tak potom v pr�pade potreby vyvolan� kompil�cia dan�ho zdrojov�ho s�boru.
*
* Program xgettext je vyu��van� na automatick� "vytiahnutie" re�azcov, ktor� bud� lokalizovan� a podporuj� tak
* viacjazy�nos�. Dodato�n� preklad a vytvorenie bin�rneho lokaliza�n�ho s�boru je potrebn� vykona� manu�lne.
*
* <hr>
* @section Poznamky Pozn�mky Release
* 
* - v0.1b (12.11.2006)
*   - Opraven� v�etky podstatn� bugy, sfunk�nenie prep�na�ov
* - v0.1a (4.11.2006) 
*   - jednoduch� defragment�cia, manipuluje sa iba s 1 clusterom. Nie je funk�n� kontrola FAT syst�mu, pracuje sa iba
*     so syst�mom FAT32. Algoritmus ako vo windowse (natla�� v�etko na za�iatok)
*
* <hr>
* @section Poziadavky Po�iadavky na syst�m
* Program bol nap�san� v debian linuxe jadro 2.6.17; je v�ak funk�n� pre hocijak� distrib�ciu UNIX/LINUX, ktor� obsahuje
* n�stroje gcc, make, xgettext, sed.
*
*/

