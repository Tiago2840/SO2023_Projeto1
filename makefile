buildbase:
	gcc -o BaseVersion baseVersion.c

buildadvanced:
	gcc -o AdvancedVersion advancedVersion.c

buildall:
	buildbase; buildadvanced