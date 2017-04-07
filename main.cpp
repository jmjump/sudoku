#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "CLI.h"

#include "Permutator.h"

#include "sudoku.h"

static void testPermutator () {
	int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

	Permutator permutator(9);
	for (int numInPermutation=0; numInPermutation<=9; numInPermutation++) {
		TRACE(0, "\n");
		TRACE(0, "Permutations with %d values from a set with %d values:\n", numInPermutation, ArraySize(values));
		permutator.reset();

		for (int i=0; i<ArraySize(values); i++) {
			permutator.setValue(values[i]);
		}

		permutator.setNumInPermutation(numInPermutation);

		
		int* permutation;
		for (int i=1; permutation = permutator.getNextPermutation(); i++) {
			TRACE(0, "Permutation %d:\n", i);
			for (int j=0; j<numInPermutation; j++) {
				TRACE(0, "    %d: %d\n", j, permutation[j]);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

static void printHelp (const char* argv0) {
	printf("%s [OPTIONS] <filename> [<filename> ...]\n", argv0);
	printf("    -v : print version information\n");
	printf("    -d : increase trace level\n");
	printf("    -D <level> : set the trace level\n");
	printf("    -s : run the solver\n");

	exit(0);
}

static void printVersion (const char* argv0) {
	static const char* version = "0.1";

	printf("%s version %s (%s %s)\n", argv0, version, __DATE__, __TIME__);

	exit(0);
}

SudokuSolver* g_solver;

////////////////////////////////////////////////////////////////////////////////
#define QUOTE(...) #__VA_ARGS__

struct TestCase {
	const char*				m_testDescription;
	const char*				m_gameFilename;
	const char*				m_solutionFilename;
} g_testCases [] = {
	"naked singles", "nakedSingles.txt", "nakedSingles.solution.txt",
	"hidden singles", "hiddenSingles.txt", "hiddenSingles.solution.txt",
	"naked pairs", "nakedPairs.txt", "nakedPairs.solution.txt",
	"hidden pairs", "hiddenPairs.txt", "hiddenPairs.solution.txt",
	"naked triples", "nakedTriples.txt", "nakedTriples.solution.txt",
	"hidden triples", "hiddenTriples.txt", "hiddenTriples.solution.txt",
	"locked candidates (pointing)", "lockedCandidates-pointing.txt", "lockedCandidates-pointing.solution.txt",
	"locked candidates (claiming)", "lockedCandidates-claiming.txt", "lockedCandidates-claiming.solution.txt",
	"X-wings", "xwing-row.txt", "xwing-row.solution.txt",
	"singles chain", "singlesChain1.txt", "singlesChain1.solution.txt",
	"Y-wings", "ywing.txt", "ywing.solution.txt",
	"swordfish", "swordfish.txt", "swordfish.solution.txt",
	"XYZ-wings", "xyz-wing.txt", "xyz-wing.solution.txt",
};

static void testSolver () {
	for (int i=0; i<ArraySize(g_testCases); i++) {
		TestCase* testCase = &g_testCases[i];

		const char* testDescription = testCase->m_testDescription;
		const char* gameFilename = testCase->m_gameFilename;
		const char* solutionFilename = testCase->m_solutionFilename;

		TRACE(0, "Test case: %s\n", testDescription);
		TRACE(0, "    loading file(%s)\n", gameFilename);

		int status;
		if ((status = g_solver->loadGameFile(gameFilename)) < 0) {
			TRACE(0, "error: unable to load '%s'\n", gameFilename);
		} else {
			TRACE(0, "    running solver\n");
			g_solver->solve();

			TRACE(0, "    checking solution(%s)\n", solutionFilename);
			status = g_solver->checkGameFile(solutionFilename);
		}

		TRACE(0, "    %s %s\n", testDescription, (status == 0 ? "PASSED" : "FAILED"));

		if (status < 0) {
			exit(0);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

static void processGame (CLI* cli) {
	char* filename = cli->getStringParameter();
	if (!filename) {
		return;
	}

	g_solver->loadGameFile(filename);
}

static void processPrint (CLI* cli) {
	int level = cli->getIntParameter(false, 1);

	g_solver->print(level);
}

static void processStep (CLI* cli) {
	int numSteps = cli->getIntParameter(false, 1);

	for (int i=0; i<numSteps; i++) {
		bool status = g_solver->tryToSolve();

		if (!status) {
			printf("No more steps\n");
			break;
		} else {
			g_solver->print();
		}
	}
}

static void processRun (CLI* cli) {
	g_solver->solve();
}

static void processAlgorithm (CLI* cli) {
	int alg = cli->getIntParameter(false, -1);

	if (alg == -1) {
		g_solver->listAlgorithms();
	} else {
		g_solver->runAlgorithm((AlgorithmType)alg);
	}
}

static void processValidate (CLI* cli) {
	g_solver->validate(1);
}

static void processTest (CLI* cli) {
	testSolver();
}

////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
	bool runSolver = false;
	bool runUnitTests = false;

	int opt;
    while ((opt = getopt(argc, argv, "hvdD:st")) != EOF) {
        if (opt == 'h') {
            printHelp(argv[0]);
        } else if (opt == 'v') {
            printVersion(argv[0]);
        } else if (opt == 'd') {
            g_debugLevel++;
        } else if (opt == 'D') {
            g_debugLevel = atoi(optarg);
        } else if (opt == 's') {
			runSolver = true;
		} else if (opt == 't') {
			runUnitTests = true;
		}
    }

	if (runUnitTests) {
		//testPermutator(); // TBD: make this a real test!
	}

	g_solver = new SudokuSolver();

	if (runUnitTests) {
		testSolver();
	}

	if (optind < argc) {
        const char* filename = argv[optind];
		g_solver->loadGameFile(filename);
		g_solver->print();
		if (runSolver) {
			g_solver->solve();
			exit(0);
		}
	}

	CLI cli;

	cli.addCommand("game", processGame, "<filename> : load a game from the specified file");
	cli.addCommand("print", processPrint, "[<detail level>] : print the game board");
	cli.addCommand("step", processStep, "[<numSteps>] : run the algorithm numSteps times (default=1)");
	cli.addCommand("run", processRun, "run the solver to completion");
	cli.addCommand("alg", processAlgorithm, "[<alg>] : run the specified algorithm");
	cli.addCommand("validate", processValidate, "validate the puzzle");
	cli.addCommand("test", processTest, "run unit tests");

	cli.processInput(stdin);
}
