# Commands to build the code files
buildbase:
	gcc -o BaseVersion baseVersion.c

buildadvanced:
	gcc -o AdvancedVersion advancedVersion.c

buildoriginal:
	gcc -o OriginalVersion originalVersion.c

buildall: buildbase buildadvanced buildoriginal

# Commands to run a quick test
base:
	./BaseVersion ./testfiles/ex8.txt 10 3
	
advanced:
	./AdvancedVersion ./testfiles/ex8.txt 10 3

original:
	./OriginalVersion ./testfiles/ex8.txt 10 3

# Commands to build and run
execbase: buildbase base

execadvanced: buildadvanced advanced

execoriginal: buildoriginal original

execall: execbase execadvanced execoriginal

# Command to clean up the compiled files
clean:
	rm -f BaseVersion AdvancedVersion OriginalVersion