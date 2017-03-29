#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "Common.h"
#include "Permutator.h"

#include "sudoku.h"

static const char* algorithmToString (AlgorithmType algorithm) {
	switch (algorithm) {
		case ALG_CHECK_FOR_NAKED_SINGLES: return "naked singles";
		case ALG_CHECK_FOR_HIDDEN_SINGLES: return "hidden singles";
		case ALG_CHECK_FOR_NAKED_PAIRS: return "naked pairs";
		case ALG_CHECK_FOR_HIDDEN_PAIRS: return "hidden pairs";
		case ALG_CHECK_FOR_LOCKED_CANDIDATES: return "locked candidates";
		case ALG_CHECK_FOR_XWINGS: return "X-wings";
		case ALG_CHECK_FOR_YWINGS: return "Y-wings";
		default:
			break;
	}

	return "unknown";
}

static const char* collectionToString (CollectionType collection) {
	switch (collection) {
		case ROW_COLLECTION: return "Row";
		case COL_COLLECTION: return "Col";
		case BOX_COLLECTION: return "Box";
		default:
			break;
	}

	return "unknown";
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

void Cell::setValue (int value) {
	TRACE(3, "%s(row=%d, col=%d, value=%d)\n",
		__CLASSFUNCTION__, m_row+1, m_col+1, value+1);

	m_known = true;
	m_value = value;

	// Tell each row, col and box that this value is "taken"
	for (int i=0; i<NUM_COLLECTIONS; i++) {
		CellSet* cellSet = m_cellSets[i];

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
	TRACE(3, "%s(this=%s, value=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), value+1);

	if (getPossible(value)) {
		TRACE(2, "%s(this=%s, candidate=%d) %s cannot be a %d\n",
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
	
	for (int i=0; i<NUM_COLLECTIONS; i++) {
		CellSet* cellSet = m_cellSets[i];

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

	for (int i=0; i<NUM_COLLECTIONS; i++) {
		CellSet* cellSet = m_cellSets[i];

		TRACE(3, "%s(this=%s, c=%d, otherCell=%s) checking %s\n",
			__CLASSFUNCTION__, m_name.c_str(), c+1, otherCell->getName().c_str(),
			cellSet->getName().c_str());

		for (int location=0; location<g_N; location++) {
			Cell* yetAnotherCell = cellSet->getCell(location);

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

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		CellSet* cellSet = m_cellSets[collection];

		for (int location=0; location<g_N; location++) {
			Cell* thirdCell = cellSet->getCell(location);
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

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		CellSet* cellSet = m_cellSets[collection];

		for (int location=0; location<g_N; location++) {
			Cell* secondCell = cellSet->getCell(location);
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

	m_cells.resize(g_N);

	reset();
}

void CellSet::reset () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	for (int i=0; i<g_N; i++) {
		m_isTaken[i] = false;
	}
}

void CellSet::setTaken (int value) {
	m_isTaken[value] = true;

	for (int i=0; i<g_N; i++) {
		Cell* cell = getCell(i);
		cell->setTaken(value);
	}
}

int CellSet::findMatchingCells (Cell* cellToMatch, Cell* matchingCells[]) {
	int count = 0;

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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
	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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
	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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

	for (int i=0; i<g_N; i++) {
		Cell* cell = m_cells[i];

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
		for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
			Cell* cell = *it;
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
	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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
			TRACE(1, "%s(this=%s, candidate=%d, location=%d) %s cannot be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), candidate+1, location+1, cell->getName().c_str(), candidate+1);
			anyReductions = true;
		}
	}

	return anyReductions;
}

////////////////////////////////////////////////////////////////////////////////

// X-Wing rule:
// When there are:
//	1) only two possible cells for a value in each of two different rows,
//  2) and these candidates also lie in the same column,
// then all other candidates for this value in the columns can be eliminated
bool AllCellSets::checkForXWings (int candidate) {
	TRACE(3, "%s(this=%s, candidate=%d)\n", __CLASSFUNCTION__, m_name.c_str(), candidate+1);

	bool anyReductions = false;

	// Check for CellSets that have "candidate" in exactly 2 locations
	for (CellSetVectorIterator it1=m_cellSets.begin(); it1 != m_cellSets.end(); it1++) {
		CellSet* cellSet1 = *it1;

		int cellSet1_locations[g_N];
		int numLocations = cellSet1->getLocationsForCandidate(candidate, cellSet1_locations);
		TRACE(3, "%s(this=%s, candidate=%d) trying %s: %d\n",
			__CLASSFUNCTION__, m_name.c_str(), candidate+1, cellSet1->getName().c_str(), numLocations);

		if (numLocations == 2) {
			CellSetVectorIterator it2 = it1;
			for (++it2; it2 != m_cellSets.end(); it2++) {
				CellSet* cellSet2 = *it2;

				int cellSet2_locations[g_N];
				int numLocations = cellSet2->getLocationsForCandidate(candidate, cellSet2_locations);
				TRACE(3, "%s(this=%s, candidate=%d) trying %s: %d\n",
					__CLASSFUNCTION__, m_name.c_str(), candidate+1, cellSet2->getName().c_str(), numLocations);
				if (numLocations != 2) {
					continue;
				}

				// Do the locations match? If not, keep looking!
				if ((cellSet1_locations[0] != cellSet2_locations[0]) ||
					(cellSet1_locations[1] != cellSet2_locations[1])) {
					TRACE(3, "%s(this=%s, candidate=%d) locations don't match\n",
						__CLASSFUNCTION__, m_name.c_str(), candidate+1);

					continue;
				}

				TRACE(2, "%s(this=%s, candidate=%d) found an XWing, check for reductions\n",
					__CLASSFUNCTION__, m_name.c_str(), candidate+1);
				TRACE(2, "    %s locations %d and %d\n",
					cellSet1->getName().c_str(), cellSet1_locations[0]+1, cellSet1_locations[1]+1);
				TRACE(2, "    %s locations %d and %d\n",
					cellSet2->getName().c_str(), cellSet2_locations[0]+1, cellSet2_locations[1]+1);

				for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
					CellSet* cellSet = *it;

					if ((cellSet == cellSet1) || (cellSet == cellSet2)) {
						continue;
					}

					anyReductions |= cellSet->checkForXWingReductions(candidate, numLocations, cellSet1_locations);
				}

				break; // TBD: could probably be a "return anyReductions;" (we've found an XWing for this candidate, how can there be any more?)
			}
		}
	}

	return anyReductions;
}

bool AllCellSets::checkForXWings () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	for (int candidate=0; candidate<g_N; candidate++) {
		anyReductions |= checkForXWings(candidate);
	}

	return anyReductions;
}

bool AllCellSets::checkForLockedCandidates () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		anyReductions |= cellSet->checkForLockedCandidates();
	}

	return anyReductions;
}

void AllCellSets::reset () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;
		cellSet->reset();
	}
}

bool AllCellSets::checkForNakedSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		anyChanges |= cellSet->checkForNakedSubsets(n);
	}

	return anyChanges;
}

bool AllCellSets::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		anyChanges |= cellSet->checkForHiddenSubsets(n);
	}

	return anyChanges;
}

bool AllCellSets::validate (int level) {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool valid = true;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

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

void SudokuSolver::init (const char* filename) {
	FILE* fp = fopen(filename, "r");
	if (!fp) {
		TRACE(0, "Error: unable to open \"%s\"\n", filename);
		return;
	}

	reset();

	char buffer[100]; // TBD: make variable
	for (int row=0; row<g_N; ) {
		if (!fgets(buffer, sizeof(buffer), fp)) {
			break;
		}
		if (strlen(buffer) < g_N) {
			continue;
		}

		int col=0;
		for (int i=0; col<g_N; i++) {
			if (buffer[i] == ' ') {
				continue;
			}
			if (buffer[i] != '-') {
				int value;
				sscanf(&buffer[i], "%1d", &value);
				m_allCells.setValue(row, col, value-1);
			}

			col++;
		}

		row++;
	}

	fclose(fp);

	validate();
}

void SudokuSolver::reset () {
	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		AllCellSets* allCellSets = m_allCellSets[collection];

		allCellSets->reset();
	}
}

bool SudokuSolver::checkForNakedSubsets (int n) {
	TRACE(3, "%s(n=%d)\n", __CLASSFUNCTION__, n);

	bool anyChanges = false;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		AllCellSets* allCellSets = m_allCellSets[collection];

		anyChanges |= allCellSets->checkForNakedSubsets(n);
	}

	return anyChanges;
}

bool SudokuSolver::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(n=%d)\n", __CLASSFUNCTION__, n);

	bool anyChanges = false;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		AllCellSets* allCellSets = m_allCellSets[collection];

		anyChanges |= allCellSets->checkForHiddenSubsets(n);
	}

	return anyChanges;
}

bool SudokuSolver::checkForLockedCandidates () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyChanges = false;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		AllCellSets* allCellSets = m_allCellSets[collection];

		anyChanges |= allCellSets->checkForLockedCandidates();
	}

	return anyChanges;
}

bool SudokuSolver::checkForXWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyChanges = false;

	anyChanges |= m_allRows.checkForXWings();
	anyChanges |= m_allCols.checkForXWings();

	return anyChanges;
}

bool SudokuSolver::checkForYWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	return m_allCells.checkForYWings();
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

		case ALG_CHECK_FOR_XWINGS:
			return checkForXWings();

		case ALG_CHECK_FOR_YWINGS:
			return checkForYWings();

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

std::string CellSet::toString (int level) {
	std::string str;

	int col=0;
	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

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
		for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
			Cell* cell = *it;

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

void AllCellSets::print (int level) {
	int row=0;
	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;
		std::string str = cellSet->toString(level);

		if ((row++ % g_n) == (g_n - 1)) {
			str += "\n";
		}

		printf("%s\n", str.c_str());
	}
}

void SudokuSolver::print (int level) {
	m_allRows.print(level);
}

bool SudokuSolver::validate (int level) {
	bool valid = true;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		AllCellSets* setOfCellSets = m_allCellSets[collection];

		valid &= setOfCellSets->validate(level);
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
