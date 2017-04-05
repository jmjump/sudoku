#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>

#include <vector>

#include "Common.h"
#include "Permutator.h"

#include "sudoku.h"

////////////////////////////////////////////////////////////////////////////////

// TBD: varName stays in scope
#define ForEachInArray(elementType, arrayName, varName)			\
	elementType varName = arrayName[0];							\
	for (int varName ## _i=0; varName ## _i<ArraySize(arrayName); varName=arrayName[++varName ## _i])

#define ForEachInCellArray(arrayName, varName)					\
	ForEachInArray(Cell*, arrayName, varName)

#define ForEachInCellSetArray(arrayName, varName)				\
	ForEachInArray(CellSet*, arrayName, varName)

#define ForEachInCellSetCollectionArray(arrayName, varName)		\
	ForEachInArray(CellSetCollection*, arrayName, varName)


////////////////////////////////////////////////////////////////////////////////

typedef struct {
	int			m_value;
	const char* m_name;
} NameValuePair;

const char* getNameForValue (int value, int numNameValuePairs, NameValuePair nameValuePairs[]) {
	for (int i=0; i<numNameValuePairs; i++) {
		if (nameValuePairs[i].m_value == value) {
			return nameValuePairs[i].m_name;
		}
	}

	return "unknown";
}

const char* algorithmToString (AlgorithmType algorithm) {
	NameValuePair algorithmNames[] = {
		ALG_CHECK_FOR_NAKED_SINGLES, "naked singles",
		ALG_CHECK_FOR_HIDDEN_SINGLES, "hidden singles",
		ALG_CHECK_FOR_NAKED_PAIRS, "naked pairs",
		ALG_CHECK_FOR_HIDDEN_PAIRS, "hidden pairs",
		ALG_CHECK_FOR_LOCKED_CANDIDATES, "locked candidates",
		ALG_CHECK_FOR_NAKED_TRIPLES, "naked triples",
		ALG_CHECK_FOR_HIDDEN_TRIPLES, "hidden triples",
		ALG_CHECK_FOR_XWINGS, "X-wings",
		ALG_CHECK_FOR_YWINGS, "Y-wings",
		ALG_CHECK_FOR_SINGLES_CHAINS, "single's chains",
		ALG_CHECK_FOR_SWORDFISH, "swordfish",
		ALG_CHECK_FOR_NAKED_QUADS, "naked quads",
		ALG_CHECK_FOR_HIDDEN_QUADS, "hidden quads",
		ALG_CHECK_FOR_XYZ_WINGS, "XYZ wings",
	};

	return getNameForValue(algorithm, ArraySize(algorithmNames), algorithmNames);
}

const char* collectionToString (CollectionType collection) {
	NameValuePair collectionNames[] = {
		ROW_COLLECTION, "Row",
		COL_COLLECTION, "Col",
		BOX_COLLECTION, "Box",
	};

	return getNameForValue(collection, ArraySize(collectionNames), collectionNames);
}

const char* chainStatusToString (ChainStatusType chainStatus) {
	NameValuePair chainStatusNames[] = {
		CHAIN_STATUS_UNCOLORED, "uncolored",
		CHAIN_STATUS_COLOR_RED, "red",
		CHAIN_STATUS_COLOR_BLACK, "black",
	};

	return getNameForValue(chainStatus, ArraySize(chainStatusNames), chainStatusNames);
}

static const char* cellListToString (int numCells, Cell* cells[]) {
	static char buffer[256];
	char* p = buffer;

	p += sprintf(p, "[");
	char* separator = "";
	for (int i=0; i<numCells; i++) {
		Cell* cell = cells[i];
		p += sprintf(p, "%s%s", separator, cell->getName().c_str());
		separator = ",";
	}
	p += sprintf(p, "]");


	return buffer;
}

static const char* intListToString (int n, int values[]) {
	static char buffer[100];
	char* p = buffer;

	p += sprintf(p, "[");
	char* separator = "";
	for (int i=0; i<n; i++) {
		p += sprintf(p, "%s%d", separator, values[i]+1);
		separator = ",";
	}
	p += sprintf(p, "]");

	return buffer;
}

static bool isValueOnList (int value, int n, int values[]) {
	for (int i=0; i<n; i++) {
		if (values[i] == value) {
			return true;
		}
	}

	return false;
}

// return true if the specified cell is on the list of N cells
static bool isCellOnList (Cell* cell, int N, Cell* cellList[]) {
	for (int i=0; i<N; i++) {
		if (cellList[i] == cell) {
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

Cell::Cell (int row, int col) {
	m_row = row;
	m_col = col;
	m_box = (row / g_n) + (col / g_n);
	
	m_name = makeString("R%dC%d", m_row+1, m_col+1);

	reset();
}

void Cell::reset () {
	m_known = false;

	for (int i=0; i<g_N; i++) {
		m_possibles[i] = true;
	}
}

void Cell::setValue (int value) {
	TRACE(3, "%s(row=%d, col=%d, value=%d)\n",
		__CLASSFUNCTION__, m_row+1, m_col+1, value+1);

	m_known = true;
	m_value = value;

	// Tell each row, col and box that this value is "taken"
	ForEachInCellSetArray(m_cellSets, cellSet) {
		cellSet->setTaken(value);
	}
}

int Cell::getPossibleValues (int possibles[]) {
	int numPossibleValues = 0;

	for (int i=0; i<g_N; i++) {
		if (m_possibles[i]) {
			TRACE(4, "%s(this=%s) can be %d\n",
				__CLASSFUNCTION__, m_name.c_str(), i+1);

			if (possibles) {
				possibles[numPossibleValues] = i;
			}

			numPossibleValues++;
		}
	}

	TRACE(4, "%s(this=%s) num possible values=%d\n",
		__CLASSFUNCTION__, m_name.c_str(), numPossibleValues);

	return numPossibleValues;
}

// does the "otherCell" have the same possible values as this cell
bool Cell::haveSamePossibles (Cell* otherCell) {
	TRACE(3, "%s(this=%s, otherCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str());

	for (int i=0; i<g_N; i++) {
		if (otherCell->m_possibles[i] && !m_possibles[i]) {
			TRACE(3, "%s(this=%s, otherCell=%s) position %d is not possible in this cell\n",
				__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str(), i+1);

			return false;
		}
	}

	return true;
}

// does the otherCell have exactly the same possible values as this cell
bool Cell::haveExactPossibles (Cell* otherCell) {
	TRACE(3, "%s(this=%s, otherCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str());

	for (int i=0; i<g_N; i++) {
		if (otherCell->m_possibles[i] != m_possibles[i]) {
			TRACE(3, "%s(this=%s, otherCell=%s) position %d is different\n",
				__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str(), i+1);

			return false;
		}
	}

	return true;
}
bool Cell::haveAnyOverlappingPossibles (Cell* otherCell) {
	TRACE(3, "%s(this=%s, otherCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str());

	for (int i=0; i<g_N; i++) {
		if (otherCell->m_possibles[i] && m_possibles[i]) {
			TRACE(3, "%s(this=%s, otherCell=%s) position %d is possible in both cells\n",
				__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str(), i+1);

			return true;
		}
	}

	return false;
}

bool Cell::tryToReduce (int value) {
	TRACE(4, "%s(this=%s, value=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), value+1);

	if (getPossible(value)) {
		TRACE(3, "%s(this=%s, candidate=%d) %s cannot be a %d\n",
			__CLASSFUNCTION__, m_name.c_str(), value+1, m_name.c_str(), value+1);

		m_possibles[value] = false;
		return true;
	}

	return false;
}

// We *think* this is a naked single. Make sure!
void Cell::processNakedSingle () {
	TRACE(3, "%s(this=%s)\n",
		__CLASSFUNCTION__, m_name.c_str());

	if (m_known) {
		TRACE(0, "    ERROR! value already known (%d)\n", m_value+1);
	} else {
		int onlyValue = -1;

		for (int i=0; i<g_N; i++) {
			TRACE(3, "    %d: %s BE\n", i+1, m_possibles[i] ? "CAN" : "CAN'T");

			if (m_possibles[i]) {
				if (onlyValue != -1) {
					TRACE(0, "    ERROR! not a naked single (%d and %d)\n", onlyValue+1, i+1);
					return;
				} else {
					onlyValue = i;
				}
			}
		}

		if (onlyValue == -1) {
			TRACE(0, "    ERROR! not a naked single (no possible values remain)\n");
		} else {
			TRACE(1, "%s(this=%s) %s must be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), m_name.c_str(), onlyValue+1);

			setValue(onlyValue);
		}
	}
}

bool Cell::checkForNakedReductions (Cell* nakedCell) {
	TRACE(3, "%s(this=%s, nakedCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), nakedCell->getName().c_str());

	bool anyReductions = false;

	// Any "possible" values in the nakedCall are no longer possible in this cell
	for (int value=0; value<g_N; value++) {
		if (nakedCell->getPossible(value) && this->getPossible(value)) {
			TRACE(1, "%s(this=%s, nakedCell=%s) %s cannot be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), nakedCell->getName().c_str(), m_name.c_str(), value+1);

			this->setTaken(value);
			anyReductions = true;
		}
	}

	return anyReductions;
}

bool Cell::areAnyOfTheseValuesUntaken (int n, int values[]) {
	TRACE(3, "%s(this=%s, n=%d, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), n, intListToString(n, values));

	for (int i=0; i<n; i++) {
		int value = values[i];

		if (getPossible(value)) {
			TRACE(3, "%s(this=%s, n=%d) value[%d]=%d is possible\n",
				__CLASSFUNCTION__, m_name.c_str(), n, i, value+1);

			return true;
		}
	}

	return false;
}

// this cell cannot be anything other than a value on the list
bool Cell::hiddenSubsetReduction (int n, int values[]) {
	TRACE(3, "%s(this=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), intListToString(n, values));

	// Does this cell contain any of these n values?
	// If so, then it *can't* be any *other* value!

	bool anyChanges = false;

	for (int value=0; value<g_N; value++) {
		if (!m_possibles[value]) {
			continue;
		}

		// If this value is not on the list, we can reduce!
		if (!isValueOnList(value, n, values)) {
			TRACE(1, "%s(this=%s, n=%d, values=%s) %s cannot be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), n, intListToString(n, values), m_name.c_str(), value+1);

			m_possibles[value] = false;
			anyChanges = true;
		}
	}

	return anyChanges;
}

bool Cell::hiddenSubsetReduction2 (int n, Cell* candidateCells[], int values[]) {
	TRACE(3, "%s(this=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), intListToString(n, values));

	bool anyChanges = false;

	for (int i=0; i<ArraySize(m_cellSets); i++) {
		CellSet* cellSet = m_cellSets[i];

		anyChanges |= cellSet->hiddenSubsetReduction2(n, candidateCells, values);
	}

	return anyChanges;
}

// return true if otherCell is in the same row, col or box as this cell.
// otherwise, false
bool Cell::hasNeighbor (Cell* otherCell) {
	TRACE(3, "%s(this=%s, otherCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str());

	ForEachInCellSetArray(m_cellSets, cellSet) {
		if (cellSet->hasCandidateCell(otherCell)) {
			return true;
		}
	}

	return false;
}

// Find all of the cells that are "neighbors" of both this cell and otherCell.
// For each of them, eliminate "c" as a possibility
bool Cell::checkForYWingReductions (int c, Cell* otherCell) {
	TRACE(3, "%s(this=%s, c=%d, otherCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1, otherCell->getName().c_str());

	bool anyChanges = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		TRACE(3, "%s(this=%s, c=%d, otherCell=%s) checking %s\n",
			__CLASSFUNCTION__, m_name.c_str(), c+1, otherCell->getName().c_str(),
			cellSet->getName().c_str());

		ForEachInCellArray(cellSet->m_cells, yetAnotherCell) {
			if ((yetAnotherCell == this) || (yetAnotherCell == otherCell)) {
				continue;
			}

			if (!yetAnotherCell->hasNeighbor(otherCell)) {
				continue;
			}

			if (yetAnotherCell->tryToReduce(c)) {
				TRACE(1, "%s(this=%s, c=%d, otherCell=%s) %s cannot be a %d\n",
					__CLASSFUNCTION__, m_name.c_str(), c+1, otherCell->getName().c_str(),
					yetAnotherCell->getName().c_str(), c+1);

				anyChanges = true;
			}
		}
	}

	return anyChanges;
}

bool Cell::checkForYWings (Cell* secondCell) {
	TRACE(3, "%s(this=%s) secondCell=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), secondCell->getName().c_str());

	Cell* firstCell = this;
	int firstCellPossibles[g_N];
	int numFirstCellPossibles = firstCell->getPossibleValues(firstCellPossibles);

	int secondCellPossibles[g_N];
	int numSecondCellPossibles = secondCell->getPossibleValues(secondCellPossibles);
	if (numSecondCellPossibles != 2) {
		return false;
	}

	// Need to overlap by 1
	int a, b, c;
	if ((firstCellPossibles[0] == secondCellPossibles[0]) && (firstCellPossibles[1] != secondCellPossibles[1])) {
		a = firstCellPossibles[0];
		b = firstCellPossibles[1];
		c = secondCellPossibles[1];
	} else if ((firstCellPossibles[0] != secondCellPossibles[0]) && (firstCellPossibles[1] == secondCellPossibles[1])) {
		a = firstCellPossibles[1];
		b = firstCellPossibles[0];
		c = secondCellPossibles[0];
	} else {
		return false;
	}

	// "a" is shared between the two cells
	// "b" is only in this cell
	// "c" is only in otherCell

	TRACE(3, "%s(this=%s) secondCell=%s also 2 possible values=%s (a=%d,b=%d,c=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), secondCell->getName().c_str(),
		intListToString(numSecondCellPossibles, secondCellPossibles), a+1, b+1, c+1);

	bool anyChanges = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, thirdCell) {
			if ((thirdCell == firstCell) || (thirdCell == secondCell)) {
				continue;
			}

			int thirdCellPossibles[g_N];
			int numThirdCellPossibles = thirdCell->getPossibleValues(thirdCellPossibles);
			if (numThirdCellPossibles != 2) {
				continue;
			}

			TRACE(3, "%s(this=%s) thirdCell=%s also 2 possible values=%s\n",
				__CLASSFUNCTION__, m_name.c_str(), thirdCell->getName().c_str(),
				intListToString(numThirdCellPossibles, thirdCellPossibles));

			// Make sure the values overlap correctly!
			// thirdCell *MUST* be "b" and "c"
			if ((thirdCellPossibles[0] == b) && (thirdCellPossibles[1] == c)) {
				; // thirdCell = [b,c]
			} else if ((thirdCellPossibles[1] == b) && (thirdCellPossibles[0] == c)) {
				; // thirdCell = [c, b]
			} else {
				continue;
			}

TRACE(2, "%s(this=%s) firstCell=%s has 2 possible values=%s\n",
__CLASSFUNCTION__, m_name.c_str(), firstCell->getName().c_str(),
intListToString(numFirstCellPossibles, firstCellPossibles));

TRACE(2, "%s(this=%s) secondCell=%s has 2 possible values=%s (a=%d,b=%d,c=%d)\n",
__CLASSFUNCTION__, m_name.c_str(), secondCell->getName().c_str(),
intListToString(numSecondCellPossibles, secondCellPossibles), a+1, b+1, c+1);

TRACE(2, "%s(this=%s) thirdCell=%s also 2 possible values=%s\n",
__CLASSFUNCTION__, m_name.c_str(), thirdCell->getName().c_str(),
intListToString(numThirdCellPossibles, thirdCellPossibles));

			anyChanges = secondCell->checkForYWingReductions(c, thirdCell);
		}
	}

	return anyChanges;
}

bool Cell::checkForYWings () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	int possibles[g_N];
	int numPossible = getPossibleValues(possibles);
	if (numPossible != 2) {
		return false;
	}

	TRACE(3, "%s(this=%s) has 2 possible values=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), intListToString(numPossible, possibles));

	bool anyChanges = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, secondCell) {
			if (secondCell == this) {
				continue;
			}

			anyChanges |= checkForYWings(secondCell);
		}
	}

	return anyChanges;
}

////////////////////////////////////////////////////////////////////////////////

CellSet::CellSet (CollectionType collection, std::string name) {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, name.c_str());

	m_collection = collection;
	m_name = name;
}

std::string CellSet::toString (int level) {
	std::string str;

	int col=0;
	ForEachInCellArray(m_cells, cell) {
		bool known = cell->getKnown();
		if (known) {
			int value = cell->getValue();
			str += makeString("%d", value+1);
		} else {
			str += "-";
		}

		if ((col++ % g_n) == (g_n - 1)) {
			str += " ";
		}
	}

	// more detail?
	if ((level > 0) || (g_debugLevel > 0)) {
		str += "     ";
		int col=0;
		ForEachInCellArray(m_cells, cell) {
			str += "[";

			bool known = cell->getKnown();
			if (known) {
				str += "         ";
			} else {
				for (int i=0; i<g_N; i++) {
					if (cell->getPossible(i)) {
						str += makeString("%d", i+1);
					} else {
						str += "-";
					}
				}
			}

			str += "]";

			if ((col++ % g_n) == (g_n - 1)) {
				str += "   ";
			}
		}
	}

	return str;
}

void CellSet::reset () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	for (int i=0; i<g_N; i++) {
		m_isTaken[i] = false;
	}

	ForEachInCellArray(m_cells, cell) {
		cell->reset();
	}
}

void CellSet::setTaken (int value) {
	m_isTaken[value] = true;

	ForEachInCellArray(m_cells, cell) {
		cell->setTaken(value);
	}
}

int CellSet::findMatchingCells (Cell* cellToMatch, Cell* matchingCells[]) {
	int count = 0;

	ForEachInCellArray(m_cells, cell) {
		if (cell->getKnown()) {
			continue;
		}

		// Does this cell have *exactly* the same possible values as cellToMatch?
		if (cell->haveExactPossibles(cellToMatch)) {
			matchingCells[count++] = cell;
		}
	}

	return count;
}

bool CellSet::tryToReduce (int N, Cell* matchingCells[]) {
	bool anyReductions = false;

	ForEachInCellArray(m_cells, cell) {
		if (cell->getKnown()) {
			continue;
		}

		// Is this cell on the list of matching cells?
		if (isCellOnList(cell, N, matchingCells)) {
			continue;
		}

		// cell is not on the list, therefore we can check for reductions!
		anyReductions |= cell->checkForNakedReductions(matchingCells[0]);
	}

	return anyReductions;
}

bool CellSet::checkForNakedSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n", __CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	ForEachInCellArray(m_cells, cell) {
		// If the cell is already filled in, don't bother!
		if (cell->getKnown()) {
			continue;
		}

		// How many possibilities for this cell?
		if (cell->getPossibleValues() == n) {
			TRACE(3, "    %s has %d possible value(s)\n", cell->getName().c_str(), n);

			// Short circuit! For n=1, we have a winner!
			if (n == 1) {
				cell->processNakedSingle();
				anyChanges = true;
			} else {
				Cell* matchingCells[g_N];
				int numMatchingCells = findMatchingCells(cell, matchingCells);
				if (numMatchingCells == n) {
					anyChanges |= tryToReduce(n, matchingCells);
				}
			}
		}
	}

	TRACE(3, "%s() return %s\n",
		__CLASSFUNCTION__, anyChanges ? "TRUE" : "FALSE");

	return anyChanges;
}

int CellSet::getUntakenNumbers (int untakenNumbers[]) {
	int numUntaken = 0;

	for (int i=0; i<g_N; i++) {
		if (!m_isTaken[i]) {
			if (untakenNumbers) {
				untakenNumbers[numUntaken] = i;
			}
			numUntaken++;
		}
	}

	return numUntaken;
}

// return true if the candidateCell belongs to this CellSet, otherwise false
bool CellSet::hasCandidateCell (Cell* candidateCell) {
	ForEachInCellArray(m_cells, cell) {
		if (cell == candidateCell) {
			return true;
		}
	}

	return false;
}

// return true if all of the candidateCells belong to this CellSet, otherwise false
bool CellSet::hasAllCandidateCells (int n, Cell* candidateCells[]) {
	for (int i=0; i<n; i++) {
		Cell* cell = candidateCells[i];

		if (!hasCandidateCell(cell)) {
			return false;
		}
	}

	return true;
}

bool CellSet::hiddenSubsetReduction2 (int n, Cell* candidateCells[], int values[]) {
	TRACE(3, "%s(this=%s, n=%d, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), n, intListToString(n, values));

	// Does this CellSet have *ALL* of the candidateCells?
	// If not, nothing to do here.
	if (!hasAllCandidateCells(n, candidateCells)) {
		return false;
	}

	bool anyChanges = false;

	// This CellSet contains all of the candidateCells.
	// Any cell NOT in the list of candidateCells, can NOT have any of the values in the permutation
	ForEachInCellArray(m_cells, cell) {
		if (isCellOnList(cell, n, candidateCells)) {
			continue;
		}

		for (int i=0; i<n; i++) {
			int value = values[i];

			anyChanges |= cell->tryToReduce(value);
		}
	}

	return anyChanges;
}

bool CellSet::checkForHiddenSubsets (int n, int permutation[]) {
	TRACE(3, "%s(this=%s, n=%d, permutation=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), n, intListToString(n, permutation));

	bool anyChanges = false;

	// Are there exactly N cells that contain any of the N values in the permutation?
	int count = 0;
	Cell* candidateCells[g_N];

	ForEachInCellArray(m_cells, cell) {
		if (cell->areAnyOfTheseValuesUntaken(n, permutation)) {
			candidateCells[count++] = cell;
TRACE(3, "    cell %s contains at least one of these values (count=%d)\n", cell->getName().c_str(), count);

			// short circuit!
			if (count > n) {
				break;
			}
		}
	}

	if (count == n) {
		TRACE(3, "%s(this=%s, n=%d, permutation=%s) subset found\n",
			__CLASSFUNCTION__, m_name.c_str(), n, intListToString(n, permutation));

		// The candidateCells can *only* have the values in the permutation
		for (int i=0; i<n; i++) {
			anyChanges |= candidateCells[i]->hiddenSubsetReduction(n, permutation);
		}

		// Any CellSet that has *all* of the candidate cells, might also have a reduction!
		anyChanges |= candidateCells[0]->hiddenSubsetReduction2(n, candidateCells, permutation);
	}

	TRACE(3, "%s() return %s\n",
		__CLASSFUNCTION__, anyChanges ? "TRUE" : "FALSE");

	return anyChanges;
}

bool CellSet::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n", __CLASSFUNCTION__, m_name.c_str(), n);
	TRACE(3, "    %s\n", toString(1).c_str()); // toString displays the possible values

	// How many untaken numbers are there?
	int untakenNumbers[g_N];
	int numUntaken = getUntakenNumbers(untakenNumbers);
	TRACE(3, "%s(this=%s, n=%d) untakenNumbers=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), n, intListToString(numUntaken, untakenNumbers));

	if (numUntaken < n) {
		return false;
	}

	Permutator permutator(g_N);
	for (int i=0; i<numUntaken; i++) {
		permutator.setValue(untakenNumbers[i]);
	}
	permutator.setNumInPermutation(n);

	bool anyChanges = false;
	int* permutation;
	while (permutation = permutator.getNextPermutation()) {
		anyChanges |= checkForHiddenSubsets(n, permutation);
	}

	return anyChanges;
}

// For this row/col/box, are the cells in the locations list all in the same collection?
bool CellSet::inSameRCB (CollectionType collection, int numLocations, int locations[]) {
	TRACE(3, "%s(this=%s, collection=%s, locations=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), collectionToString(collection), intListToString(numLocations, locations));

	Cell* cell = m_cells[locations[0]];
	int rcb = cell->getRCB(collection);

	for (int i=1; i<numLocations; i++) {
		cell = m_cells[locations[i]];

		if (cell->getRCB(collection) != rcb) {
			return false;
		}
	}

	return true;
}

bool CellSet::lockedCandidateReduction (int candidate, int numCells, Cell* cells[]) {
	TRACE(3, "%s(this=%s, candidate=%d, cells=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, cellListToString(numCells, cells));

	bool anyReductions = false;

	ForEachInCellArray(m_cells, cell) {
		if (isCellOnList(cell, numCells, cells)) {
			continue;
		}

		if (cell->tryToReduce(candidate)) {
			TRACE(1, "%s(this=%s, candidate=%d, cells=%s) %s cannot be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), candidate+1, cellListToString(numCells, cells),
				cell->getName().c_str(), candidate+1);
			anyReductions = true;
		}
	}

	return anyReductions;
}

bool CellSet::checkForLockedCandidate2 (int candidate, CollectionType collection, int numLocations, int locations[]) {
	TRACE(3, "%s(this=%s, candidate=%d, collection=%s, locations=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, collectionToString(collection), intListToString(numLocations, locations));

	if (!inSameRCB(collection, numLocations, locations)) {
		return false;
	}

	TRACE(2, "%s(this=%s, candidate=%d, collection=%s, locations=%s) same collection\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, collectionToString(collection), intListToString(numLocations, locations));

	// translate the "locations" to cells
	Cell* cells[g_N];
	for (int i=0; i<numLocations; i++) {
		cells[i] = m_cells[locations[i]];
	}

	// Get the appropriate CellSet
	CellSet* cellSet = cells[0]->getCellSet(collection);

	// See if the CellSet can make any reductions
	return cellSet->lockedCandidateReduction(candidate, numLocations, cells);
}

// For this row/col/box, see if the candidate number is "locked"
bool CellSet::checkForLockedCandidate (int candidate) {
	TRACE(3, "%s(this=%s, candidate=%d)\n", __CLASSFUNCTION__, m_name.c_str(), candidate+1);

	// What are the locations for this candidate?
	int locations[g_N];
	int numLocations = getLocationsForCandidate(candidate, locations);

	TRACE(3, "%s(this=%s, candidate=%d) locations=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, intListToString(numLocations, locations));

	// 0 = not found, so nothing to do
	// 1 = naked single, it'll get picked up later
	// >3, can't be in the same row/col/box
	if ((numLocations < 2) || (numLocations > 3)) {
		return false;
	}

	bool anyReductions = false;

	// If this is a box, then check to see if the locations are all in the same row or col.
	// Otherwise, if it's a row or col, see if the locations are all in the same box
	if (m_collection == BOX_COLLECTION) {
		anyReductions |= checkForLockedCandidate2(candidate, ROW_COLLECTION, numLocations, locations);
		anyReductions |= checkForLockedCandidate2(candidate, COL_COLLECTION, numLocations, locations);
	} else {
		anyReductions |= checkForLockedCandidate2(candidate, BOX_COLLECTION, numLocations, locations);
	}

	return anyReductions;
}

// For this row/col/box, see if any of the numbers are "locked"
bool CellSet::checkForLockedCandidates () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	for (int candidate=0; candidate<g_N; candidate++) {
		anyReductions |= checkForLockedCandidate(candidate);
	}

	return anyReductions;
}

bool CellSet::validate (int level) {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool valid = true;

	char debug[100];
	char* p = debug;

	for (int i=0; i<g_N; i++) {
		int count = 0;
		ForEachInCellArray(m_cells, cell) {
			if (cell->getKnown() && (cell->getValue() == i)) {
				count++;
			}
		}

		if (count == 0) {
			p+=sprintf(p, "-");
		} else if (count == 1) {
			p+=sprintf(p, "%d", i+1);
		} else {
			p+=sprintf(p, "X");
		}

		if (count > 1) {
			TRACE(0, "%s(%s) Error >1 %d's\n", __CLASSFUNCTION__, m_name.c_str(), i+1);
		}
	}

	if (level > 0) {
		TRACE(0, "%s: %s\n", m_name.c_str(), debug);
	}

	return valid;
}

int CellSet::getLocationsForCandidate (int candidate, int locations[]) {
	TRACE(3, "%s(this=%s, candidate=%d)\n", __CLASSFUNCTION__, m_name.c_str(), candidate+1);

	int count = 0;

	int location = 0;
	ForEachInCellArray(m_cells, cell) {
		if (cell->getPossible(candidate)) {
			locations[count++] = location;
		}

		location++;
	}

	return count;
}

bool CellSet::checkForXWingReductions (int candidate, int numLocations, int locations[]) {
	TRACE(3, "%s(this=%s, candidate=%d, locations=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, intListToString(numLocations, locations));

	bool anyReductions = false;

	for (int i=0; i<numLocations; i++) {
		int location = locations[i];

		// If "candidate" is possible in location, then it can be eliminated!
		Cell* cell = m_cells[location];

		if (cell->tryToReduce(candidate)) {
			TRACE(1, "%s(this=%s, candidate=%d, locations=%s) %s cannot be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), candidate+1,
				intListToString(numLocations, locations), cell->getName().c_str(), candidate+1);
			anyReductions = true;
		}
	}

	return anyReductions;
}

////////////////////////////////////////////////////////////////////////////////

typedef struct {
	CellSet*		m_cellSet;
	int				m_locations[g_N];
} XWingInfo;

// X-Wing rule:
// When there are:
//	1) only "n" possible cells for a value in each of "n" different rows/cols,
//  2) and these candidates also lie in the same column/row,
// then all other candidates for this value in those columns/rows can be eliminated
bool CellSetCollection::checkForXWings (int n, int candidate) {
	TRACE(3, "%s(this=%s, n=%d, candidate=%d)\n", __CLASSFUNCTION__, m_name.c_str(), n, candidate+1);

	// Check for CellSets that have "candidate" in exactly n locations
	XWingInfo xWingInfo[g_N];
	int numCellSetsWithCandidateInNLocations = 0;
	ForEachInCellSetArray(m_cellSets, cellSet) {
		int locations[g_N];
		int numLocations = cellSet->getLocationsForCandidate(candidate, locations);
		TRACE(3, "%s(this=%s, candidate=%d) trying %s: locations=%s\n",
			__CLASSFUNCTION__, m_name.c_str(), candidate+1, cellSet->getName().c_str(),
			intListToString(numLocations, locations));

		// If this CellSet has candidate in exactly "n" locations, then save that info
		if (numLocations == n) {
			XWingInfo* info = &xWingInfo[numCellSetsWithCandidateInNLocations++];
			info->m_cellSet = cellSet;
			memcpy(info->m_locations, locations, sizeof(locations));
		}
	}

	bool anyReductions = false;

	// n.b., we found numCellSetsWithCandidateInNLocations CellSets.
	// If this number is less than "n", then we're finished.
	// If this number is equal to "n", then we need to see if the locations in all "n" match.
	// If this number is greater than "n", then it's more complicated. We have to check
	//     for all combinations of "n" CellSets to see if *any* match. Sorry 'bout that!
	for (int i=0; i<numCellSetsWithCandidateInNLocations-n+1; i++) {
		XWingInfo* infoToMatch = &xWingInfo[i];

		CellSet* matchingCellSets[g_N];
		int numMatches = 0;
		matchingCellSets[numMatches++] = infoToMatch->m_cellSet;
		for (int j=i+1; j<numCellSetsWithCandidateInNLocations; j++) {
			XWingInfo* info = &xWingInfo[j];

			if (memcmp(infoToMatch->m_locations, info->m_locations, n * sizeof(info->m_locations[0])) == 0) {
				matchingCellSets[numMatches++] = info->m_cellSet;
			}
		}

		// Did we have exactly "n" matches?
		if (numMatches == n) {
			ForEachInCellSetArray(m_cellSets, cellSet) {
				// Make sure it's not one of the "matching" cellSets
				bool isOneOfTheMatchingCellSets = false;
				for (int i=0; i<n; i++) {
					if (cellSet == matchingCellSets[i]) {
						isOneOfTheMatchingCellSets = true;
						break;
					}
				}
				if (isOneOfTheMatchingCellSets) {
					continue;
				}

				bool status = cellSet->checkForXWingReductions(candidate, n, infoToMatch->m_locations);
				if (status) {
					TRACE(2, "%s(this=%s, n=%d, candidate=%d) locations=%s\n",
						__CLASSFUNCTION__, m_name.c_str(), n, candidate+1,
						intListToString(n, infoToMatch->m_locations));
					for (int i=0; i<n; i++) {
						CellSet* cs = matchingCellSets[i];
						TRACE(2, "    %s\n", cs->getName().c_str());
					}

					anyReductions = true;
				}
			}
		}
	}

	return anyReductions;
}

void CellSetCollection::print (int level) {
	int row=0;
	ForEachInCellSetArray(m_cellSets, cellSet) {
		std::string str = cellSet->toString(level);

		if ((row++ % g_n) == (g_n - 1)) {
			str += "\n";
		}

		printf("%s\n", str.c_str());
	}
}

bool CellSetCollection::checkForLockedCandidates () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		anyReductions |= cellSet->checkForLockedCandidates();
	}

	return anyReductions;
}

void CellSetCollection::reset () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	ForEachInCellSetArray(m_cellSets, cellSet) {
		cellSet->reset();
	}
}

bool CellSetCollection::checkForNakedSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		anyChanges |= cellSet->checkForNakedSubsets(n);
	}

	return anyChanges;
}

bool CellSetCollection::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		anyChanges |= cellSet->checkForHiddenSubsets(n);
	}

	return anyChanges;
}

bool CellSetCollection::validate (int level) {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool valid = true;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		valid &= cellSet->validate(level);
	}

	return valid;
}

////////////////////////////////////////////////////////////////////////////////

bool AllCells::checkForYWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyChanges = false;

	for (int i=0; i<ArraySize(m_cells)-2; i++) {
		anyChanges |= m_cells[i]->checkForYWings();
	}

	return anyChanges;
}

////////////////////////////////////////////////////////////////////////////////

SudokuSolver::SudokuSolver () {
	m_cellSetCollections[ROW_COLLECTION] = &m_allRows;
	m_cellSetCollections[COL_COLLECTION] = &m_allCols;
	m_cellSetCollections[BOX_COLLECTION] = &m_allBoxes;

	// initialize the collections
	for (int row=0; row<g_N; row++) {
		for (int col=0; col<g_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
				cellSetCollection->setCell(row, col, cell);
			}
		}
	}

	reset(); // for good measure
}

int SudokuSolver::loadGameString (const char* gameString) {
	reset();

	for (int row=0; row<g_N; ) {
		for (int col=0; col<g_N; gameString++) {
			if (*gameString == '-') {
				// blank
			} else if ((*gameString >= '1') && (*gameString <= '9')) {
				int value = *gameString - '1';
				m_allCells.getCell(row, col)->setValue(value);
			} else {
				// anything else is window dressing
				continue;
			}

			col++;
		}

		row++;
	}

	 return validate() ? 0 : -1;
}

int SudokuSolver::checkGameString (const char* gameString) {
	for (int row=0; row<g_N; ) {
		for (int col=0; col<g_N; gameString++) {
			// ignore whitespace
			if (isspace(*gameString)) {
				continue;
			}

			if ((*gameString >= '1') && (*gameString <= '9')) {
				int correctValue = *gameString - '1';

				int gameValue = m_allCells.getCell(row, col)->getValue();

				if (gameValue != correctValue) {
					TRACE(0, "%s() error: value[row=%d][col=%d]=%d != %d\n",
						__CLASSFUNCTION__, row+1, col+1, gameValue+1, correctValue+1);
					return -1;
				}
			} else {
				TRACE(0, "%s() error in row %d col %d, ('%c')\n",
					__CLASSFUNCTION__, row+1, col+1, *gameString);
				return -1;
			}

			col++;
		}

		row++;
	}

	return 0;
}

char* readGameFile (const char* filename) {
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		TRACE(0, "Error: unable to open \"%s\"\n", filename);
		return NULL;
	}

	#define MIN_GAME_FILE_SIZE (g_N*g_N)
	static char buffer[MIN_GAME_FILE_SIZE + 100];
	ssize_t readLen = read(fd, buffer, sizeof(buffer));
	close(fd);

	if (readLen < MIN_GAME_FILE_SIZE) {
		TRACE(0, "Error: file not big enough (%d)\n", readLen);
		return NULL;
	}

	return buffer;
}

int SudokuSolver::loadGameFile (const char* filename) {
	char* buffer = readGameFile(filename);

	return buffer ? loadGameString(buffer) : -1;
}

int SudokuSolver::checkGameFile (const char* filename) {
	char* buffer = readGameFile(filename);

	return buffer ? checkGameString(buffer) : -1;
}

void SudokuSolver::reset () {
	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		cellSetCollection->reset();
	}
}

bool SudokuSolver::checkForNakedSubsets (int n) {
	TRACE(3, "%s(n=%d)\n", __CLASSFUNCTION__, n);

	bool anyChanges = false;

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		anyChanges |= cellSetCollection->checkForNakedSubsets(n);
	}

	return anyChanges;
}

bool SudokuSolver::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(n=%d)\n", __CLASSFUNCTION__, n);

	bool anyChanges = false;

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		anyChanges |= cellSetCollection->checkForHiddenSubsets(n);
	}

	return anyChanges;
}

bool SudokuSolver::checkForLockedCandidates () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyChanges = false;

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		anyChanges |= cellSetCollection->checkForLockedCandidates();
	}

	return anyChanges;
}

bool SudokuSolver::checkForXWings (int n) {
	TRACE(3, "%s(n=%d)\n", __CLASSFUNCTION__, n);

	bool anyChanges = false;

	for (int candidate=0; candidate<g_N; candidate++) {
		anyChanges |= m_allRows.checkForXWings(n, candidate);
		anyChanges |= m_allCols.checkForXWings(n, candidate);
	}

	return anyChanges;
}

bool SudokuSolver::checkForYWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	return m_allCells.checkForYWings();
}

void Cell::resetChain () {
	m_chainStatus = CHAIN_STATUS_UNCOLORED;
}

void AllCells::resetChains () {
	ForEachInCellArray(m_cells, cell) {
		cell->resetChain();
	}
}

int CellSet::getNumPossible (int candidate) {
	TRACE(3, "%s(this=%s, candidate=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1);

	int numPossible = 0;

	ForEachInCellArray(m_cells, cell) {
		if (cell->getPossible(candidate)) {
			numPossible++;
		}
	}

	TRACE(4, "%s(this=%s, candidate=%d) numPossible=%d\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, numPossible);

	return numPossible;
}

bool Cell::isConjugatePair (int candidate) {
	TRACE(3, "%s(this=%s, candidate=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1);

	ForEachInCellSetArray(m_cellSets, cellSet) {
		int numPossible = cellSet->getNumPossible(candidate);
		if (numPossible == 2) {
TRACE(2, "%s(this=%s, candidate=%d) is conjugate pair in %s\n", __CLASSFUNCTION__, m_name.c_str(), candidate+1, cellSet->getName().c_str());
			return true;
		}
	}

	return false;
}

bool Cell::buildChains (int candidate, ChainStatusType chainStatus, Cell* linkingCell) {
	TRACE(4, "%s(this=%s, candidate=%d, chainStatus=%s, linkingCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, chainStatusToString(chainStatus),
		linkingCell ? linkingCell->getName().c_str() : "NULL");

	// If this cell has already been colored, bail
	if (m_chainStatus != CHAIN_STATUS_UNCOLORED) {
		return false;
	}

	// If this cell does not have candidate for a possible value, then bail
	if (!getPossible(candidate)) {
		return false;
	}

	// Is this cell part of a conjugate pair?
	if (!isConjugatePair(candidate)) {
		return false;
	}

	// Set this cell's status
	m_chainStatus = chainStatus;

	TRACE(3, "%s(this=%s, candidate=%d, chainStatus=%s, linkingCell=%s) %s=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, chainStatusToString(chainStatus),
		linkingCell ? linkingCell->getName().c_str() : "NULL",
		m_name.c_str(), chainStatusToString(chainStatus));

	ChainStatusType nextStatus = (chainStatus == CHAIN_STATUS_COLOR_RED) ?
		CHAIN_STATUS_COLOR_BLACK :
		CHAIN_STATUS_COLOR_RED;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		cellSet->buildChains(candidate, nextStatus, this);
	}

	return true;
}

void CellSet::buildChains (int candidate, ChainStatusType chainStatus, Cell* linkingCell) {
	TRACE(4, "%s(this=%s, candidate=%d, chainStatus=%s, linkingCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, chainStatusToString(chainStatus), linkingCell->getName().c_str());

	// Are there only 2 possible cells for this candidate?
	// If so, then there is a chain!
	if (getNumPossible(candidate) != 2) {
		return;
	}

	TRACE(3, "%s(this=%s, candidate=%d, chainStatus=%s, linkingCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, chainStatusToString(chainStatus), linkingCell->getName().c_str());

	ForEachInCellArray(m_cells, cell) {
		cell->buildChains(candidate, chainStatus, linkingCell);
	}
}


// return true if any of the cells in this CellSet have the specified chainStatus
bool CellSet::checkChainStatus (ChainStatusType chainStatus) {
	ForEachInCellArray(m_cells, cell) {
		if (cell->getChainStatus() == chainStatus) {
			return true;
		}
	}

	return false;
}

// return true if this (uncolored) cell can see a cell of the specified chainStatus
bool Cell::canSee (ChainStatusType chainStatus) {
	ForEachInCellSetArray(m_cellSets, cellSet) {
		if (cellSet->checkChainStatus(chainStatus)) {
			return true;
		}
	}

	return false;
}

void SudokuSolver::printColors () {
	for (int row=0; row<g_N; row++) {
		for (int col=0; col<g_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			if (cell->getChainStatus() == CHAIN_STATUS_COLOR_RED) {
				printf("r");
			} else if (cell->getChainStatus() == CHAIN_STATUS_COLOR_BLACK) {
				printf("b");
			} else {
				printf("-");
			}
		}
		printf("\n");
	}
	printf("\n");
}

// Any cells with a matching chainStatus can have "candidate" removed
bool SudokuSolver::singlesChainsReduction (int candidate, ChainStatusType chainStatus) {
	TRACE(2, "%s(candidate=%d, chainStatus=%s)\n",
		__CLASSFUNCTION__, candidate+1, chainStatusToString(chainStatus));

	bool anyChanges = false;

	for (int row=0; row<g_N; row++) {
		for (int col=0; col<g_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			if (cell->getChainStatus() != chainStatus) {
				continue;
			}

			if (cell->tryToReduce(candidate)) {
				TRACE(1, "%s(candidate=%d, chainStatus=%s) %s cannot be a %d\n",
					__CLASSFUNCTION__, candidate+1, chainStatusToString(chainStatus), cell->getName().c_str(), candidate+1);

				anyChanges = true;
			}
		}
	}

	return anyChanges;
}

// How many cells in this CellSet have the specified chainStatus?
int CellSet::getChainStatusCount (ChainStatusType chainStatus) {
	int count = 0;

	ForEachInCellArray(m_cells, cell) {
		if (cell->getChainStatus() == chainStatus) {
			count++;
		}
	}

	return count;
}

// return true if two cells in this CellSet have the specified chainStatus
bool CellSetCollection::checkForTwoOfTheSameColor (ChainStatusType chainStatus) {
	TRACE(3, "%s(this=%s, chainStatus=%s)\n", __CLASSFUNCTION__, m_name.c_str(), chainStatusToString(chainStatus));

	ForEachInCellSetArray(m_cellSets, cellSet) {
		int chainStatusCount = cellSet->getChainStatusCount(chainStatus);

		TRACE(3, "%s(this=%s, chainStatus=%s) %s count=%d\n",
			__CLASSFUNCTION__, m_name.c_str(), chainStatusToString(chainStatus), cellSet->getName().c_str(), chainStatusCount);

		if (chainStatusCount == 2) {
			return true;
		}
	}

	return false;
}

bool SudokuSolver::checkForSinglesChainsReductions_SameColor (int candidate, ChainStatusType chainStatus) {
	TRACE(2, "%s(candidate=%d, chainStatus=%s)\n", __CLASSFUNCTION__, candidate+1, chainStatusToString(chainStatus));

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		if (cellSetCollection->checkForTwoOfTheSameColor(chainStatus)) {
			return singlesChainsReduction(candidate, chainStatus);
		}
	}

	return false;
}

// If any uncolored cells can see cells of different colors, then the specified
// candidate is no longer possible
bool SudokuSolver::checkForSinglesChainsReductions_DifferentColors (int candidate) {
	TRACE(3, "%s(candidate=%d)\n", __CLASSFUNCTION__, candidate+1);

	bool anyChanges = false;

	for (int row=0; row<g_N; row++) {
		for (int col=0; col<g_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			if (cell->getChainStatus() != CHAIN_STATUS_UNCOLORED) {
				continue;
			}

			// If this uncolored cell can see two cells with different colors, then it can't be "candidate"
			if (cell->canSee(CHAIN_STATUS_COLOR_RED) && cell->canSee(CHAIN_STATUS_COLOR_BLACK)) {
				if (cell->tryToReduce(candidate)) {
					TRACE(1, "%s(candidate=%d) %s cannot be a %d\n",
						__CLASSFUNCTION__, candidate+1, cell->getName().c_str(), candidate+1);

					anyChanges = true;
				}
			}
		}
	}

	return anyChanges;
}

bool SudokuSolver::checkForSinglesChains (int candidate) {
	TRACE(3, "%s(candidate=%d)\n", __CLASSFUNCTION__, candidate+1);

	m_allCells.resetChains();

	bool anyChanges = false;

	for (int row=0; row<g_N; row++) {
		for (int col=0; col<g_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			if (cell->buildChains(candidate, CHAIN_STATUS_COLOR_RED, NULL)) {
				bool status = false;

				// Do any of the CellSet's have two of the same color?
				status |= checkForSinglesChainsReductions_SameColor(candidate, CHAIN_STATUS_COLOR_RED);
				status |= checkForSinglesChainsReductions_SameColor(candidate, CHAIN_STATUS_COLOR_BLACK);

				// If not, check for any cells that see two cells of different colors
				if (!status) {
					status = checkForSinglesChainsReductions_DifferentColors(candidate);
				}

				if (status) {
					printColors();
					anyChanges = true;
				}

				m_allCells.resetChains();
			}
		}
	}

	return anyChanges;
}

bool SudokuSolver::checkForSinglesChains () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyChanges = false;

	for (int candidate=0; candidate<g_N; candidate++) {
		anyChanges |= checkForSinglesChains(candidate);
	}

	return anyChanges;
}

bool Cell::checkForXYZWings () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	Cell* firstCell = this;

	int firstCellPossibleValues[g_N];
	int numFirstCellPossibleValues = firstCell->getPossibleValues(firstCellPossibleValues);
	if (numFirstCellPossibleValues != 3) {
		return false;
	}

TRACE(1, "%s(this=%s) has 3 possible values: %s\n", __CLASSFUNCTION__, m_name.c_str(), intListToString(numFirstCellPossibleValues, firstCellPossibleValues));

	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, secondCell) {
		//for (int location=0; location<g_N; location++) {
			//Cell* secondCell = cellSet->getCell(location);

			int secondCellPossibleValues[g_N];
    		int numSecondCellPossibleValues = secondCell->getPossibleValues(secondCellPossibleValues);
    		if (numSecondCellPossibleValues != 2) {
				continue;
    		}

TRACE(1, "%s(this=%s) %s has 2 possible values: %s\n", __CLASSFUNCTION__, m_name.c_str(),
secondCell->getName().c_str(), intListToString(numSecondCellPossibleValues, secondCellPossibleValues));

			// Make sure the two values of the second cell match two of the three values of the first cell
			if (!isValueOnList(secondCellPossibleValues[0], numFirstCellPossibleValues, firstCellPossibleValues) ||
				!isValueOnList(secondCellPossibleValues[1], numFirstCellPossibleValues, firstCellPossibleValues)) {
				continue;
			}

TRACE(1, "    so far, so good!\n");


		}
	}

	return false;
}

bool AllCells::checkForXYZWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyChanges = false;

	ForEachInCellArray(m_cells, cell) {
		anyChanges |= cell->checkForXYZWings();
	}

	return anyChanges;
}

bool SudokuSolver::checkForXYZWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	return m_allCells.checkForXYZWings();
}

bool SudokuSolver::runAlgorithm (AlgorithmType algorithm) {
	TRACE(3, "%s(algorithm=%s)\n", __CLASSFUNCTION__, algorithmToString(algorithm));

	switch (algorithm) {
		case ALG_CHECK_FOR_NAKED_SINGLES:
			return checkForNakedSubsets(1);

		case ALG_CHECK_FOR_HIDDEN_SINGLES:
			return checkForHiddenSubsets(1);

		case ALG_CHECK_FOR_NAKED_PAIRS:
			return checkForNakedSubsets(2);

		case ALG_CHECK_FOR_HIDDEN_PAIRS:
			return checkForHiddenSubsets(2);

		case ALG_CHECK_FOR_LOCKED_CANDIDATES:
			return checkForLockedCandidates();

		case ALG_CHECK_FOR_NAKED_TRIPLES:
			return checkForNakedSubsets(3);

		case ALG_CHECK_FOR_HIDDEN_TRIPLES:
			return checkForHiddenSubsets(3);

		case ALG_CHECK_FOR_XWINGS:
			return checkForXWings(2);

		case ALG_CHECK_FOR_YWINGS:
			return checkForYWings();

		case ALG_CHECK_FOR_SINGLES_CHAINS:
			return checkForSinglesChains();

		case ALG_CHECK_FOR_SWORDFISH:
			return checkForXWings(3);

		case ALG_CHECK_FOR_NAKED_QUADS:
			return checkForNakedSubsets(4);

		case ALG_CHECK_FOR_HIDDEN_QUADS:
			return checkForHiddenSubsets(4);

		case ALG_CHECK_FOR_XYZ_WINGS:
			return checkForXYZWings();

		default:
			break;
	}

	return false;
}

bool SudokuSolver::tryToSolve () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	// Try each of the various algorithms. Stop when one is successful.
	for (int i=0; i<NUM_ALGORITHMS; i++) {
		AlgorithmType algorithm = (AlgorithmType)i;

		if (runAlgorithm(algorithm)) {
			if (!validate()) {
				TRACE(0, "%s(algorithm=%s) INVALID solution!\n",
					__CLASSFUNCTION__, algorithmToString(algorithm));
			}

			return true;
		}

		TRACE(1, "%s(algorithm=%s) no changes\n", __CLASSFUNCTION__, algorithmToString(algorithm));
	}

	return false;
}

void SudokuSolver::solve () {
	while (1) {
		print(g_debugLevel);

		if (isSolved()) {
			break;
		}

		if (!tryToSolve()) {
// TBD: unsolved!
print();
			break;
		}
	}
}

void SudokuSolver::print (int level) {
	m_allRows.print(level);
}

bool SudokuSolver::validate (int level) {
	bool valid = true;

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		valid &= cellSetCollection->validate(level);
	}

	TRACE((valid ? 2 : 0), "%s() status=%s\n", __CLASSFUNCTION__, valid ? "valid" : "INVALID");

	return valid;
}

void SudokuSolver::listAlgorithms () {
	printf("Algorithm numbers:\n");
	for (int i=0; i<NUM_ALGORITHMS; i++) {
		printf("%  d: %s\n", i, algorithmToString((AlgorithmType)i));
	}
}
