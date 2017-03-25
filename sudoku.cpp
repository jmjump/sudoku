#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "Common.h"
#include "Permutator.h"

#include "sudoku.h"

const char* toString (AlgorithmType algorithm) {
	switch (algorithm) {
		case ALG_CHECK_FOR_NAKED_SINGLES: return "naked singles";
		case ALG_CHECK_FOR_HIDDEN_SINGLES: return "hidden singles";
		case ALG_CHECK_FOR_NAKED_PAIRS: return "naked pairs";
		case ALG_CHECK_FOR_HIDDEN_PAIRS: return "hidden pairs";
		case ALG_CHECK_FOR_LOCKED_CANDIDATES: return "locked candidates";
		case ALG_CHECK_FOR_XWINGS: return "X-wings";
		default:
			break;
	}

	return "unknown";
}

////////////////////////////////////////////////////////////////////////////////

static const char* listToString (int n, int values[]) {
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

static bool isValueInList (int value, int n, int values[]) {
	for (int i=0; i<n; i++) {
		if (values[i] == value) {
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void Cell::setValue (int value) {
	TRACE(2, "%s(row=%d, col=%d, value=%d)\n",
		__CLASSFUNCTION__, m_row+1, m_col+1, value+1);

	m_known = true;
	m_value = value;

	// Tell each row, col and box that this value is "taken"
	for (int i=0; i<NUM_COLLECTIONS; i++) {
		CellSet* cellSet = m_cellSets[i];

		cellSet->setTaken(value);
	}
}

int Cell::getNumPossibleValues () {
	int numPossibleValues = 0;

	for (int i=0; i<g_N; i++) {
		if (m_possibles[i]) {
			TRACE(3, "%s(this=%s) can be %d\n",
				__CLASSFUNCTION__, m_name.c_str(), i+1);

			numPossibleValues++;
		}
	}

	TRACE(3, "%s(this=%s) num possible values=%d\n",
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

bool Cell::tryToReduce (Cell* firstCell, Cell* secondCell) {
	TRACE(3, "%s(this=%s, firstCell=%s, secondCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), firstCell->getName().c_str(), secondCell->getName().c_str());

	bool anyReductions = false;

	for (int i=0; i<g_N; i++) {
		if (m_possibles[i]) {
			if (this == secondCell) {
				// if "this" is the second cell, then remove any vacancies that don't exist in firstCell
				if (firstCell->m_possibles[i]) {
					continue;
				}
			} else {
				// otherwise, remove any vacancies in common with firstCell
				if (!firstCell->m_possibles[i]) {
					continue;
				}
			}

			TRACE(1, "%s(this=%s, firstCell=%s, secondCell=%s) can no longer be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), firstCell->getName().c_str(), secondCell->getName().c_str(), i+1);

			m_possibles[i] = false;
			anyReductions = true;
		}
	}

	return anyReductions;
}

bool Cell::tryToReduce (int c) {
	TRACE(2, "%s(this=%s, c=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1);

	if (getPossible(c)) {
		TRACE(2, "%s(this=%s, candidate=%d) %s cannot be a %d\n",
			__CLASSFUNCTION__, m_name.c_str(), c+1, m_name.c_str(), c+1);

		m_possibles[c] = false;
		return true;
	}

	return false;
}

// We *think* this is a naked single. Make sure!
void Cell::processNakedSingle () {
	TRACE(2, "%s(this=%s)\n",
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

////////////////////////////////////////////////////////////////////////////////

CellSet::CellSet (CollectionType type, std::string name) {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, name.c_str());

	m_type = type;
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

// return true if the specified cell is on the list of N cells
static bool isCellOnList (Cell* cell, int N, Cell* cellList[]) {
	for (int i=0; i<N; i++) {
		if (cellList[i] == cell) {
			return true;
		}
	}

	return false;
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
	TRACE(2, "%s(this=%s, n=%d)\n", __CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

		// If the cell is already filled in, don't bother!
		if (cell->getKnown()) {
			continue;
		}

		// How many possibilities for this cell?
		if (cell->getNumPossibleValues() == n) {
			TRACE(2, "    %s has %d possible value(s)\n", cell->getName().c_str(), n);

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

	TRACE(2, "%s() return %s\n",
		__CLASSFUNCTION__, anyChanges ? "TRUE" : "FALSE");

	return anyChanges;
}

/*
bool CellSet::checkForHiddenSingles (int c) {
	TRACE(2, "%s(this=%s, c=%d)\n", __CLASSFUNCTION__, m_name.c_str(), c+1);

	int winningPosition = -1;
	for (int position=0; position<g_N; position++) {
		Cell* cell = m_cells[position];

		if (cell->getPossible(c)) {
			// If there already is one, then bail
			if (winningPosition != -1) {
				return false;
			}
			winningPosition = position;
		}
	}

	// do we have a winner?
	if (winningPosition == -1) {
		return false;
	}

	TRACE(1, "%s(this=%s, c=%d) found in position %d\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1, winningPosition+1);

	Cell* cell = m_cells[winningPosition];
	cell->setValue(c);

	return true;
}

bool CellSet::checkForHiddenSingles () {
	TRACE(2, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyChanges = false;

	for (int i=0; i<g_N; i++) {
		anyChanges |= checkForHiddenSingles(i);
	}

	return anyChanges;
}
*/

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

bool Cell::areAnyOfTheseValuesUntaken (int n, int values[]) {
	TRACE(3, "%s(this=%s, n=%d, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), n, listToString(n, values));

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

bool Cell::hiddenSubsetReduction (int n, int values[]) {
	// Does this cell contain any of these n values?
	// If so, then it *can't* be any *other* value!

	bool anyChanges = false;

	for (int value=0; value<g_N; value++) {
		if (!m_possibles[value]) {
			continue;
		}

		// If this value is not on the list, we can reduce!
		if (!isValueInList(value, n, values)) {
			TRACE(1, "%s(this=%s, n=%d, values=%s) %s cannot be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), n, listToString(n, values), m_name.c_str(), value+1);

			m_possibles[value] = false;
			anyChanges = true;
		}
	}

	return anyChanges;
}

bool CellSet::hiddenSubsetReduction (int n, int permutation[]) {
	TRACE(2, "%s(this=%s, n=%d, permutation=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), n, listToString(n, permutation));

	bool anyChanges = false;

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

		if (cell->areAnyOfTheseValuesUntaken(n, permutation)) {
			// special case for n==1
			// The cell *must* be the value of the first (and only) value on the permutation list
			if (n == 1) {
				int value = permutation[0];

				TRACE(1, "%s(this=%s, n=%d, permutation=%s) %s must be a %d\n",
					__CLASSFUNCTION__, m_name.c_str(), n, listToString(n, permutation), cell->getName().c_str(), value+1);

				cell->setValue(value);

				anyChanges = true;
			} else {
				anyChanges |= cell->hiddenSubsetReduction(n, permutation);
			}
		}
	}

	return anyChanges;
}

bool CellSet::checkForHiddenSubsets (int n, int permutation[]) {
	TRACE(3, "%s(this=%s, n=%d, permutation=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), n, listToString(n, permutation));

	bool anyChanges = false;

	// Are there exactly N cells that contain any of the N values in the permutation?
	int count = 0;

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

		if (cell->areAnyOfTheseValuesUntaken(n, permutation)) {
			count++;
TRACE(3, "    cell %s contains at least one of these values (count=%d)\n", cell->getName().c_str(), count);

			// short circuit!
			if (count > n) {
				break;
			}
		}
	}

	if (count == n) {
		TRACE(2, "%s(this=%s, n=%d, permutation=%s) subset found\n",
			__CLASSFUNCTION__, m_name.c_str(), n, listToString(n, permutation));

		anyChanges |= hiddenSubsetReduction(n, permutation);
	}

	TRACE(3, "%s() return %s\n",
		__CLASSFUNCTION__, anyChanges ? "TRUE" : "FALSE");

	return anyChanges;
}

bool CellSet::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n", __CLASSFUNCTION__, m_name.c_str(), n);
	TRACE(3, "    %s\n", toString(1).c_str());

	// How many untaken numbers are there?
	int untakenNumbers[g_N];
	int numUntaken = getUntakenNumbers(untakenNumbers);
	TRACE(3, "%s(this=%s, n=%d) untakenNumbers=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), n, listToString(numUntaken, untakenNumbers));

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

/*
PLEASE DELETE ME
int CellSet::getNumUnknownCells () {
	int numUnknownCells = 0;
	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;
		if (!cell->getKnown()) {
			numUnknownCells++;
		}
	}

	return numUnknownCells;
}
*/

/*
// Is there only 1 other cell that has the same vacancies as firstCell?
// If so, reduce!
bool CellSet::checkForPairs (Cell* firstCell) {
	TRACE(2, "%s(this=%s, firstCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), firstCell->getName().c_str());

	Cell* secondCell = NULL;

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;
		if (cell->getKnown()) {
			continue;
		}
		if (cell == firstCell) {
			continue;
		}

		if (cell->haveSamePossibles(firstCell)) { // TBD: double check the logic
			// If we already have a second cell, then that's too many!
			if (secondCell) {
				TRACE(2, "%s(this=%s, firstCell=%s) >1 cells with the same vacancies\n",
					__CLASSFUNCTION__, m_name.c_str(), firstCell->getName().c_str());

				return false;
			}

			// otherwise, record the fact that we have a second cell
			secondCell = cell;
		} else if (cell->haveAnyOverlappingPossibles(firstCell)) {
			return false;
		}
	}

	// If we didn't find a second cell, there's nothing more we can do
	if (!secondCell) {
		return false;
	}

	// There is another cell ("secondCell") that has the same vacancies as firstCell.
	// See if there are any reductions to be made!
	bool anyReductions = false;
	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;
		if (cell->getKnown()) {
			continue;
		}
		if (cell == firstCell) {
			continue;
		}

		anyReductions |= cell->tryToReduce(firstCell, secondCell);
	}

	return anyReductions;
}

bool CellSet::checkForPairs () {
	TRACE(2, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	// How many untaken numbers are there?
	int numUntakenNumbers = getUntakenNumbers();
	if (numUntakenNumbers < 3) { // TBD: function of the number we're looking for
		return false;
	}

	bool anyReductions = false;

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;
		if (cell->getKnown()) {
			continue;
		}

		// Does this cell have only 2 possible values?
		if (cell->getNumPossibleValues() == 2) { // TBD: variable
			anyReductions |= checkForPairs(cell);
		}
	}

	return anyReductions;
}
*/

void CellSet::getBoxCells (Cell* boxCells[], bool isRow, int i) {
	int pos0, pos1, pos2;

	if (isRow) {
		pos0 = i * g_n;
		pos1 = pos0 + 1;
		pos2 = pos1 + 1;
	} else {
		pos0 = i;
		pos1 = pos0 + g_n;
		pos2 = pos1 + g_n;
	}

	boxCells[0] = getCell(pos0);
	boxCells[1] = getCell(pos1);
	boxCells[2] = getCell(pos2);
}

// return "true" if the specified cell is in the specified set of cells. Otherwise, return false
bool CellSet::cellInSet (Cell* cell, Cell* cellSet[]) {
	for (int i=0; i<g_n; i++) {
		if (cell == cellSet[i]) {
			return true;
		}
	}

	return false;
}

bool CellSet::checkForLock (int c, Cell* cells[]) {
	TRACE(3, "%s(this=%s, candidate=%d, cell1=%s, cell2=%s, cell3=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1,
		cells[0]->getName().c_str(),
		cells[1]->getName().c_str(),
		cells[2]->getName().c_str());

	for (int pos=0; pos<g_N; pos++) {
		Cell* cell = getCell(pos);
		if (cell->getKnown()) {
			if (cell->getValue() == c) {
				return false;
			}
		} else if (cell->getPossible(c)) {
			// candidate(c) is possible in this cell.
			// If this cell is NOT in one of the three positions, then we don't match
			if (!cellInSet(cell, cells)) {
				TRACE(3, "%s(this=%s, candidate=%d, cell1=%s, cell2=%s, cell3=%s) no lock (cell=%s)\n",
					__CLASSFUNCTION__, m_name.c_str(), c+1,
					cells[0]->getName().c_str(),
					cells[1]->getName().c_str(),
					cells[2]->getName().c_str(),
					cell->getName().c_str());

				return false;
			}
		}
	}

	TRACE(3, "%s(this=%s, candidate=%d, cell1=%s, cell2=%s, cell3=%s) lock found!\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1,
		cells[0]->getName().c_str(),
		cells[1]->getName().c_str(),
		cells[2]->getName().c_str());

	return true;
}

// TBD: Removal -> Reduction
bool CellSet::lockCandidateRemoval (int c, Cell* boxCells[]) {
	TRACE(3, "%s(this=%s, candidate=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1);

	bool anyReductions = false;

	for (int i=0; i<g_N; i++) {
		Cell* cell = getCell(i);

		if (cell->getKnown()) {
			continue;
		}

		if (cellInSet(cell, boxCells)) {
			continue;
		}

		if (cell->tryToReduce(c)) {
			TRACE(1, "%s(this=%s, candidate=%d) %s cannot be a %d\n",
				__CLASSFUNCTION__, m_name.c_str(), c+1, cell->getName().c_str(), c+1);

			anyReductions = true;
		}
	}

	return anyReductions;
}

bool CellSet::checkForLockedCandidate (int c, int i, bool isRow) {
	TRACE(3, "%s(this=%s, candidate=%d, %s %d)\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1, (isRow ? "row" : "col"), i+1);

	Cell* boxCells[3];
	getBoxCells(boxCells, isRow, i);

	bool status = checkForLock(c, boxCells);
	if (status) {
		// get the first cell
		Cell* cell = boxCells[0];

		// get the appropriate CellSet (row or col)
		CellSet* cellSet = cell->getCellSet(isRow ? ROW_COLLECTION : COL_COLLECTION);

		return cellSet->lockCandidateRemoval(c, boxCells);
	}

	return false;
}

bool CellSet::checkForLockedCandidate (int c) {
	TRACE(3, "%s(this=%s, candidate=%d)\n", __CLASSFUNCTION__, m_name.c_str(), c+1);

	bool anyReductions = false;

	for (int i=0; i<g_n; i++) {
		anyReductions |= checkForLockedCandidate(c, i, true);
		anyReductions |= checkForLockedCandidate(c, i, false);
	}

	return anyReductions;
}

bool CellSet::checkForLockedCandidates () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	for (int c=0; c<g_N; c++) {
		anyReductions |= checkForLockedCandidate(c);
	}

	return anyReductions;
}

bool CellSet::validate () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool valid = true;

	for (int i=0; i<g_N; i++) {
		int count = 0;
		for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
			Cell* cell = *it;
			if (cell->getKnown() && (cell->getValue() == i)) {
				count++;
			}
		}

		if (count > 1) {
			TRACE(0, "%s(%s) Error >1 %d's\n", __CLASSFUNCTION__, m_name.c_str(), i+1);
		}
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

				TRACE(3, "%s(this=%s, candidate=%d) found an XWing, check for reductions\n",
					__CLASSFUNCTION__, m_name.c_str(), candidate+1);

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

bool CellSet::checkForXWingReductions (int candidate, int numLocations, int locations[]) {
	TRACE(3, "%s(this=%s, candidate=%d, locations=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, listToString(numLocations, locations));

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

bool AllCellSets::checkForXWings () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	for (int candidate=0; candidate<g_N; candidate++) {
		anyReductions |= checkForXWings(candidate);
	}

	return anyReductions;
}

////////////////////////////////////////////////////////////////////////////////

bool AllBoxes::checkForLockedCandidates () {
	TRACE(2, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		anyReductions |= cellSet->checkForLockedCandidates();
	}

	return anyReductions;
}

////////////////////////////////////////////////////////////////////////////////

void AllCellSets::reset () {
	TRACE(2, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;
		cellSet->reset();
	}
}

bool AllCellSets::checkForNakedSubsets (int n) {
	TRACE(2, "%s(this=%s, n=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		anyChanges |= cellSet->checkForNakedSubsets(n);
	}

	return anyChanges;
}

bool AllCellSets::checkForHiddenSubsets (int n) {
	TRACE(2, "%s(this=%s, n=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), n);

	bool anyChanges = false;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		anyChanges |= cellSet->checkForHiddenSubsets(n);
	}

	return anyChanges;
}

bool AllCellSets::runAlgorithm (AlgorithmType algorithm) {
	TRACE(2, "%s(this=%s, algorithm=%s)\n", __CLASSFUNCTION__, m_name.c_str(), toString(algorithm));

	switch (algorithm) {
		case ALG_CHECK_FOR_NAKED_SINGLES:
			// efficiency improvement only: only run on "rows" because that will check every cell anyway
			return (m_type == ROW_COLLECTION) ? checkForNakedSubsets(1) : false;

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

		default:
			break;
	}

	return false;
}

bool AllCellSets::validate () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool valid = true;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		valid &= cellSet->validate();
	}

	return valid;
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

bool SudokuSolver::runAlgorithm (AlgorithmType algorithm) {
	TRACE(2, "%s(algorithm=%s)\n", __CLASSFUNCTION__, toString(algorithm));

	bool anyChanges = false;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		AllCellSets* allCellSets = m_allCellSets[collection];

		anyChanges |= allCellSets->runAlgorithm(algorithm);
	}

	// Sanity check!
	bool status = validate();
	if (!status) {
		TRACE(0, "INVALID solution!\n");
	}

	return anyChanges;
}

bool SudokuSolver::tryToSolve () {
	// Try each of the various algorithms. Stop when one is successful.
	for (int algorithm=0; algorithm<NUM_ALGORITHMS; algorithm++) {
		if (runAlgorithm((AlgorithmType)algorithm)) {
			return true;
		}
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

bool SudokuSolver::validate () {
	bool valid = true;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		AllCellSets* setOfCellSets = m_allCellSets[collection];

		valid &= setOfCellSets->validate();
	}

	TRACE((valid ? 2 : 0), "%s() status=%s\n", __CLASSFUNCTION__, valid ? "valid" : "INVALID");

	return valid;
}
