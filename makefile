# Functions to build the code files
buildbase:
	gcc -o BaseVersion baseVersion.c

buildadvanced:
	gcc -o AdvancedVersion advancedVersion.c

buildoriginal:
	gcc -o OriginalVersion originalVersion.c

buildall: 
	buildbase buildadvanced buildoriginal

# Functions to run a quick test
base:
	./BaseVersion distancias.txt 10 60
	
advanced:
	./AdvancedVersion distancias.txt 10 60

original:
	./OriginalVersion distancias.txt 10 60

# Functions to build and run
execbase:
	buildbase base

execadvanced:
	buildadvanced advanced

execoriginal:
	buildoriginal original