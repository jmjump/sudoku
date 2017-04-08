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

////////////////////////////////////////////////////////////////////////////////

IntList::IntList () {
}

IntList::IntList (int n, int values[]) {
	for (int i=0; i<n; i++) {
		addValue(values[i]);
	}
}

bool IntList::operator== (IntList& other) {
	if (size() != other.size()) {
		return false;
	}

	for (int i=0; i<size(); i++) {
		if (getValue(i) != other.getValue(i)) {
			return false;
		}
	}

	return true;
}

void IntList::addValue (int value) {
	push_back(value);
}

int IntList::getValue (int i) {
	return (*this)[i];
}

int IntList::getLength () {
	return size();
}

const char* IntList::toString () {
	static char buffer[100];
	char* p = buffer;

	p += sprintf(p, "[");
	const char* separator = "";
	for (int i=0; i<size(); i++) {
		p += sprintf(p, "%s%d", separator, getValue(i)+1);
		separator = ",";
	}
	p += sprintf(p, "]");

	return buffer;
}

bool IntList::onList (int value) {
	for (int i=0; i<getLength(); i++) {
		if (getValue(i) == value) {
			return true;
		}
	}

	return false;
}

IntList IntList::findIntersection (IntList& listA, IntList& listB) {
	IntList intersection;

	for (int i=0; i<listA.getLength(); i++) {
		int value = listA.getValue(i);
		if (listB.onList(value)) {
			intersection.addValue(value);
		}
	}

	return intersection;
}

void IntList::findUnion (IntList& otherList) {
	for (int i=0; i<otherList.getLength(); i++) {
		int value = otherList.getValue(i);
		if (!onList(value)) {
			addValue(value);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CellList::addValue (Cell* cell) {
	push_back(cell);
}

Cell* CellList::getValue (int i) {
	return (*this)[i];
}

int CellList::getLength () {
	return size();
}

bool CellList::onList (Cell* cell) {
	for (int i=0; i<getLength(); i++) {
		if (getValue(i) == cell) {
			return true;
		}
	}

	return false;
}

const char* CellList::toString () {
	static char buffer[256];
	char* p = buffer;

	p += sprintf(p, "[");
	const char* separator = "";
	for (int i=0; i<getLength(); i++) {
		Cell* cell = getValue(i);
		p += sprintf(p, "%s%s", separator, cell->getName().c_str());
		separator = ",";
	}
	p += sprintf(p, "]");

	return buffer;
}

////////////////////////////////////////////////////////////////////////////////

PossibleValues::PossibleValues () {
	reset();
}

void PossibleValues::setValue (int value) {
	m_known = true;
	m_value = value;
}

void PossibleValues::setNoLongerPossible (int value) {
	m_possibleValues[value] = false;

	adjustList();
}

void PossibleValues::reset () {
	m_known = false;

	for (int i=0; i<g_N; i++) {
		m_possibleValues[i] = true;
	}

	adjustList();
}

bool PossibleValues::isPossible (int value) {
	return !m_known && m_possibleValues[value];
}

void PossibleValues::adjustList () {
	m_list.resize(0);

	if (!m_known) {
		for (int i=0; i<g_N; i++) {
			if (m_possibleValues[i]) {
				m_list.addValue(i);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

// Hack alert!
// so that deep in the bowels we know which algorithm we're working on!
// Ideally, we'd call cell->getCurrentAlgorithm(), but I'm too lazy right now
// to provide the chaining from cell back up to SudokuSover.
AlgorithmType g_currentAlgorithm;

////////////////////////////////////////////////////////////////////////////////

Cell::Cell (int row, int col) {
	m_row = row;
	m_col = col;
	m_box = (row / g_n) + (col / g_n);
	
	m_name = makeString("R%dC%d", m_row+1, m_col+1);

	reset();
}

void Cell::reset () {
	m_possibleValues.reset();
}

void Cell::adjustPossibleValuesList () {
	m_possibleValues.adjustList();
}

void Cell::setValue (int value) {
	TRACE(3, "%s(row=%d, col=%d, value=%d)\n",
		__CLASSFUNCTION__, m_row+1, m_col+1, value+1);

	m_possibleValues.setValue(value);

	// Tell each row, col and box that this value is no longer possible
	ForEachInCellSetArray(m_cellSets, cellSet) {
		cellSet->setNoLongerPossible(value);
	}
}

void Cell::setNoLongerPossible (int value) {
	m_possibleValues.setNoLongerPossible(value);
}

bool Cell::isPossible (int value) {
	return m_possibleValues.isPossible(value);
}

// does the "otherCell" have the same possible values as this cell
bool Cell::haveSamePossibles (Cell* otherCell) {
	TRACE(3, "%s(this=%s, otherCell=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str());

	for (int i=0; i<g_N; i++) {
		if (otherCell->m_possibleValues.isPossible(i) && !m_possibleValues.isPossible(i)) {
			TRACE(3, "%s(this=%s, otherCell=%s) position %d is not possible in this cell\n",
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
		if (otherCell->isPossible(i) && isPossible(i)) {
			TRACE(3, "%s(this=%s, otherCell=%s) position %d is possible in both cells\n",
				__CLASSFUNCTION__, m_name.c_str(), otherCell->getName().c_str(), i+1);

			return true;
		}
	}

	return false;
}

bool Cell::tryToReduce (IntList& values, AlgorithmType algorithm) {
	TRACE(4, "%s(this=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), values.toString());

	bool anyReductions = false;

	for (int i=0; i<values.getLength(); i++) {
		int value = values.getValue(i);

		anyReductions |= tryToReduce(value, algorithm);
	}

	return anyReductions;
}

bool Cell::tryToReduce (int value, AlgorithmType algorithm) {
	TRACE(4, "%s(this=%s, value=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), value+1);

	if (isPossible(value)) {
		if (algorithm != NUM_ALGORITHMS) {
			TRACE(1, "%s cannot be a %d (%s)\n", m_name.c_str(), value+1, algorithmToString(algorithm));
		}

		m_possibleValues.setNoLongerPossible(value);

		return true;
	}

	return false;
}

// We *think* this is a naked single. Make sure!
bool Cell::processNakedSingle () {
	TRACE(3, "%s(this=%s)\n",
		__CLASSFUNCTION__, m_name.c_str());

	if (m_possibleValues.getKnown()) {
		TRACE(0, "    ERROR! value already known (%d)\n", m_possibleValues.getValue()+1);
		return false;
	}

	int onlyValue = -1;

	for (int i=0; i<g_N; i++) {
		TRACE(3, "    %d: %s BE\n", i+1, isPossible(i) ? "CAN" : "CAN'T");

		if (isPossible(i)) {
			if (onlyValue != -1) {
				TRACE(0, "    ERROR! not a naked single (%d and %d)\n", onlyValue+1, i+1);
				return false;
			}

			onlyValue = i;
		}
	}

	if (onlyValue == -1) {
		TRACE(0, "    ERROR! not a naked single (no possible values remain)\n");
		return false;
	}

	TRACE(1, "%s must be a %d (%s)\n",
		m_name.c_str(), onlyValue+1, algorithmToString(ALG_CHECK_FOR_NAKED_SINGLES));

	setValue(onlyValue);

	return true;
}

bool Cell::areAnyOfTheseValuesPossible (IntList& values) {
	TRACE(3, "%s(this=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), values.toString());

	for (int i=0; i<values.getLength(); i++) {
		int value = values.getValue(i);

		if (isPossible(value)) {
			TRACE(3, "%s(this=%s, values=%s) value[%d]=%d is possible\n",
				__CLASSFUNCTION__, m_name.c_str(), values.toString(), i, value+1);

			return true;
		}
	}

	return false;
}

// this cell cannot be anything other than a value on the list
bool Cell::hiddenSubsetReduction (IntList& values) {
	TRACE(3, "%s(this=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), values.toString());

	// Does this cell contain any of these n values?
	// If so, then it *can't* be any *other* value!

	bool anyReductions = false;

	for (int value=0; value<g_N; value++) {
		// If this value is on the list, we can't reduce!
		if (values.onList(value)) {
			continue;
		}

		anyReductions |= tryToReduce(value, g_currentAlgorithm);
	}

	return anyReductions;
}

bool Cell::hiddenSubsetReduction2 (CellList& candidateCells, IntList& values) {
	TRACE(3, "%s(this=%s, candidateCells=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidateCells.toString(), values.toString());

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		anyReductions |= cellSet->hiddenSubsetReduction2(candidateCells, values);
	}

	return anyReductions;
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

// Find all of the cells that are "neighbors" of both this cell and cell3.
// For each of them, eliminate "candidate" as a possibility
bool Cell::checkForYWingReductions (int candidate, Cell* cell3) {
	TRACE(3, "%s(this=%s, candidate=%d, cell3=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, cell3->getName().c_str());

	Cell* cell2 = this;

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		TRACE(3, "%s(this=%s, c=%d, cell3=%s) checking %s\n",
			__CLASSFUNCTION__, m_name.c_str(), candidate+1, cell3->getName().c_str(),
			cellSet->getName().c_str());

		ForEachInCellArray(cellSet->m_cells, cell) {
			if ((cell == cell2) || (cell == cell3)) {
				continue;
			}

			if (!cell->hasNeighbor(cell3)) {
				continue;
			}

			anyReductions |= cell->tryToReduce(candidate, g_currentAlgorithm);
		}
	}

	return anyReductions;
}

bool Cell::checkForYWings (Cell* cell2) {
	TRACE(3, "%s(this=%s) cell2=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), cell2->getName().c_str());

	Cell* cell1 = this;
	IntList* cell1PossibleValues = cell1->getPossibleValuesList();

	IntList* cell2PossibleValues = cell2->getPossibleValuesList();
	if (cell2PossibleValues->getLength() != 2) {
		return false;
	}

	int a, b, c;
	if ((cell1PossibleValues->getValue(0) == cell2PossibleValues->getValue(0)) &&
		(cell1PossibleValues->getValue(1) != cell2PossibleValues->getValue(1))) {
		a = cell1PossibleValues->getValue(0);
		b = cell1PossibleValues->getValue(1);
		c = cell2PossibleValues->getValue(1);
	} else if ((cell1PossibleValues->getValue(0) != cell2PossibleValues->getValue(0)) &&
			   (cell1PossibleValues->getValue(1) == cell2PossibleValues->getValue(1))) {
		a = cell1PossibleValues->getValue(1);
		b = cell1PossibleValues->getValue(0);
		c = cell2PossibleValues->getValue(0);
	} else {
		return false;
	}

	// "a" is shared between the two cells (cell1 and cell2)
	// "b" is only in cell1
	// "c" is only in cell2

	TRACE(3, "%s(this=%s) cell2=%s also 2 possible values=%s (a=%d,b=%d,c=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), cell2->getName().c_str(),
		cell2PossibleValues->toString(), a+1, b+1, c+1);

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, cell3) {
			if ((cell3 == cell1) || (cell3 == cell2)) {
				continue;
			}

			IntList* cell3PossibleValues = cell3->getPossibleValuesList();
			if (cell3PossibleValues->getLength() != 2) {
				continue;
			}

			TRACE(3, "%s(this=%s) cell3=%s also 2 possible values=%s\n",
				__CLASSFUNCTION__, m_name.c_str(), cell3->getName().c_str(),
				cell3PossibleValues->toString());

			// Make sure the values overlap correctly!
			// cell3 *MUST* be "b" and "c"
			if ((cell3PossibleValues->getValue(0) == b) && (cell3PossibleValues->getValue(1) == c)) {
				; // cell3 = [b,c]
			} else if ((cell3PossibleValues->getValue(1) == b) && (cell3PossibleValues->getValue(0) == c)) {
				; // cell3 = [c, b]
			} else {
				continue;
			}

TRACE(2, "%s(this=%s) cell1=%s has 2 possible values=%s\n",
__CLASSFUNCTION__, m_name.c_str(), cell1->getName().c_str(),
cell1PossibleValues->toString());

TRACE(2, "%s(this=%s) cell2=%s has 2 possible values=%s\n",
__CLASSFUNCTION__, m_name.c_str(), cell2->getName().c_str(),
cell2PossibleValues->toString());

TRACE(2, "%s(this=%s) cell3=%s also 2 possible values=%s\n",
__CLASSFUNCTION__, m_name.c_str(), cell3->getName().c_str(),
cell3PossibleValues->toString());

			anyReductions = cell2->checkForYWingReductions(c, cell3);
		}
	}

	return anyReductions;
}

bool Cell::checkForYWings () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	IntList* possibleValuesList = m_possibleValues.getList();
	if (possibleValuesList->getLength() != 2) {
		return false;
	}

	TRACE(3, "%s(this=%s) has 2 possible values=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), possibleValuesList->toString());

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, cell2) {
			if (cell2 == this) {
				continue;
			}

			anyReductions |= checkForYWings(cell2);
		}
	}

	return anyReductions;
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
	if (level > 1) {
		str += "     ";
		int col=0;
		ForEachInCellArray(m_cells, cell) {
			str += "[";

			bool known = cell->getKnown();
			if (known) {
				str += "         ";
			} else {
				for (int i=0; i<g_N; i++) {
					if (cell->isPossible(i)) {
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

	m_possibleValues.reset();

	ForEachInCellArray(m_cells, cell) {
		cell->reset();
	}
}

void CellSet::setNoLongerPossible (int value) {
	m_possibleValues.setNoLongerPossible(value);

	ForEachInCellArray(m_cells, cell) {
		cell->setNoLongerPossible(value);
	}
}

bool CellSet::nakedSubsetReduction (CellList& cellList, IntList& values) {
	TRACE(3, "%s(this=%s, cellList=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), cellList.toString(), values.toString());

	bool anyReductions = false;

	ForEachInCellArray(m_cells, cell) {
		if (cell->getKnown()) {
			continue;
		}

		// Is this cell on the list of matching cells?
		if (cellList.onList(cell)) {
			continue;
		}

		anyReductions |= cell->tryToReduce(values, g_currentAlgorithm);
	}

	return anyReductions;
}

bool CellSet::checkForNakedSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n", __CLASSFUNCTION__, m_name.c_str(), n);

	// Make a list of the candidate cells:
	// 1) not known,
	// 2) <= n possible values
	Permutator permutator(g_N);
	for (int location=0; location<g_N; location++) {
		Cell* cell = m_cells[location];

		if (cell->getKnown()) {
			continue;
		}

		IntList* possibleValuesList = cell->getPossibleValuesList();
		if (possibleValuesList->getLength() > n) {
			continue;
		}

		permutator.setValue(location);
	}
	permutator.setNumInPermutation(n);

	// Make sure we have enough
	if (permutator.getNumValues() < n) {
		return false;
	}

	bool anyReductions = false;
	int* nextPermutation;
	while ((nextPermutation = permutator.getNextPermutation())) {
		// Build a CellList from the permutation
		CellList cellList;

		// Keep track of the union of the possible values from the "n" cells
		IntList possibleValuesUnion;

		for (int i=0; i<n; i++) {
			int location = nextPermutation[i];
			Cell* cell = m_cells[location];
			cellList.addValue(cell);

			IntList* possibleValues = cell->getPossibleValuesList();

			possibleValuesUnion.findUnion(*possibleValues);
		}

		if (possibleValuesUnion.getLength() == n) {
			// Special case for n==1 (hint: naked single)
			if (n == 1) {
				Cell* cell = cellList.getValue(0);
				anyReductions |= cell->processNakedSingle();
			} else {
				anyReductions |= nakedSubsetReduction(cellList, possibleValuesUnion);
			}
		}
	}

	TRACE(3, "%s() return %s\n",
		__CLASSFUNCTION__, anyReductions ? "TRUE" : "FALSE");

	return anyReductions;
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
bool CellSet::hasAllCandidateCells (CellList& candidateCells) {
	for (int i=0; i<candidateCells.getLength(); i++) {
		Cell* cell = candidateCells.getValue(i);

		if (!hasCandidateCell(cell)) {
			return false;
		}
	}

	return true;
}

bool CellSet::hiddenSubsetReduction2 (CellList& candidateCells, IntList& values) {
	TRACE(3, "%s(this=%s, candidateCells=%s, values=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidateCells.toString(), values.toString());

	// Does this CellSet have *ALL* of the candidateCells?
	// If not, nothing to do here.
	if (!hasAllCandidateCells(candidateCells)) {
		return false;
	}

	bool anyReductions = false;

	// This CellSet contains all of the candidateCells.
	// Any cell NOT in the list of candidateCells, can NOT have any of the values in the permutation
	ForEachInCellArray(m_cells, cell) {
		if (candidateCells.onList(cell)) {
			continue;
		}

		for (int i=0; i<values.getLength(); i++) {
			int value = values.getValue(i);

			anyReductions |= cell->tryToReduce(value, g_currentAlgorithm);
		}
	}

	return anyReductions;
}

bool CellSet::checkForHiddenSubsets (IntList& permutation) {
	TRACE(3, "%s(this=%s, permutation=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), permutation.toString());

	bool anyReductions = false;

	// Are there exactly N cells that contain any of the N values in the permutation?
	CellList candidateCells;
	ForEachInCellArray(m_cells, cell) {
		if (cell->areAnyOfTheseValuesPossible(permutation)) {
			candidateCells.addValue(cell);

			TRACE(3, "    cell %s contains at least one of these values (count=%d)\n",
				cell->getName().c_str(), candidateCells.getLength());
		}
	}

	if (candidateCells.getLength() == permutation.size()) {
		TRACE(3, "%s(this=%s, permutation=%s) subset found\n",
			__CLASSFUNCTION__, m_name.c_str(), permutation.toString());

		// The candidateCells can *only* have the values in the permutation
		for (int i=0; i<candidateCells.getLength(); i++) {
			Cell* candidateCell = candidateCells.getValue(i);
			anyReductions |= candidateCell->hiddenSubsetReduction(permutation);
		}

		// Any CellSet that has *all* of the candidate cells, might also have a reduction!
		anyReductions |= candidateCells.getValue(0)->hiddenSubsetReduction2(candidateCells, permutation);
	}

	TRACE(3, "%s() return %s\n",
		__CLASSFUNCTION__, anyReductions ? "TRUE" : "FALSE");

	return anyReductions;
}

bool CellSet::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n", __CLASSFUNCTION__, m_name.c_str(), n);
	TRACE(3, "    %s\n", toString(1).c_str()); // toString displays the possible values

	IntList* possibleValuesList = m_possibleValues.getList();

	// How many possible values are there?
	int numPossibleValues = possibleValuesList->size();
	TRACE(3, "%s(this=%s, n=%d) possibleValues=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), n, possibleValuesList->toString());

	if (numPossibleValues < n) {
		return false;
	}

	Permutator permutator(g_N);
	for (int i=0; i<numPossibleValues; i++) {
		permutator.setValue(possibleValuesList->getValue(i));
	}
	permutator.setNumInPermutation(n);

	bool anyReductions = false;
	int* nextPermutation;
	while ((nextPermutation = permutator.getNextPermutation())) {
		IntList permutation(n, nextPermutation); // TBD: relationship between Permutator and IntList (g_N)

		anyReductions |= checkForHiddenSubsets(permutation);
	}

	return anyReductions;
}

// For this row/col/box, are the cells in the locations list all in the same collection?
bool CellSet::inSameRCB (CollectionType collection, IntList& locations) {
	TRACE(3, "%s(this=%s, collection=%s, locations=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), collectionToString(collection), locations.toString());

	Cell* cell = m_cells[locations.getValue(0)];
	int rcb = cell->getRCB(collection);

	for (int i=1; i<locations.getLength(); i++) {
		cell = m_cells[locations.getValue(i)];

		if (cell->getRCB(collection) != rcb) {
			return false;
		}
	}

	return true;
}

bool CellSet::lockedCandidateReduction (int candidate, CellList& cellList) {
	TRACE(3, "%s(this=%s, candidate=%d, cells=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, cellList.toString());

	bool anyReductions = false;

	ForEachInCellArray(m_cells, cell) {
		if (cellList.onList(cell)) {
			continue;
		}

		anyReductions |= cell->tryToReduce(candidate, g_currentAlgorithm);
	}

	return anyReductions;
}

bool CellSet::checkForLockedCandidate2 (int candidate, CollectionType collection, IntList& locations) {
	TRACE(3, "%s(this=%s, candidate=%d, collection=%s, locations=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, collectionToString(collection), locations.toString());

	if (!inSameRCB(collection, locations)) {
		return false;
	}

	TRACE(2, "%s(this=%s, candidate=%d, collection=%s, locations=%s) same collection\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, collectionToString(collection), locations.toString());

	// translate the "locations" to cells
	CellList cellList;
	for (int i=0; i<locations.getLength(); i++) {
		int location = locations[i];

		Cell* cell = m_cells[location];

		cellList.addValue(cell);
	}

	// Get the appropriate CellSet
	CellSet* cellSet = cellList.getValue(0)->getCellSet(collection);

	// See if the CellSet can make any reductions
	return cellSet->lockedCandidateReduction(candidate, cellList);
}

// For this row/col/box, see if the candidate number is "locked"
bool CellSet::checkForLockedCandidate (int candidate) {
	TRACE(3, "%s(this=%s, candidate=%d)\n", __CLASSFUNCTION__, m_name.c_str(), candidate+1);

	// What are the locations for this candidate?
	IntList locations = getLocationsForCandidate(candidate);

	TRACE(3, "%s(this=%s, candidate=%d) locations=%s\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, locations.toString());

	// 0 = not found, so nothing to do
	// 1 = naked single, it'll get picked up later
	// >3, can't be in the same row/col/box
	if ((locations.getLength() < 2) || (locations.getLength() > 3)) {
		return false;
	}

	bool anyReductions = false;

	// If this is a box, then check to see if the locations are all in the same row or col.
	// Otherwise, if it's a row or col, see if the locations are all in the same box
	if (m_collection == BOX_COLLECTION) {
		anyReductions |= checkForLockedCandidate2(candidate, ROW_COLLECTION, locations);
		anyReductions |= checkForLockedCandidate2(candidate, COL_COLLECTION, locations);
	} else {
		anyReductions |= checkForLockedCandidate2(candidate, BOX_COLLECTION, locations);
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

IntList CellSet::getLocationsForCandidate (int candidate) {
	TRACE(3, "%s(this=%s, candidate=%d)\n", __CLASSFUNCTION__, m_name.c_str(), candidate+1);

	IntList locations;

	int location = 0;
	ForEachInCellArray(m_cells, cell) {
		if (cell->isPossible(candidate)) {
			locations.addValue(location);
		}

		location++;
	}

	return locations;
}

bool CellSet::checkForXWingReductions (int candidate, IntList& locations) {
	TRACE(3, "%s(this=%s, candidate=%d, locations=%s)\n",
		__CLASSFUNCTION__, m_name.c_str(), candidate+1, locations.toString());

	bool anyReductions = false;

	for (int i=0; i<locations.getLength(); i++) {
		int location = locations.getValue(i);

		// If "candidate" is possible in location, then it can be eliminated!
		Cell* cell = m_cells[location];

		anyReductions |= cell->tryToReduce(candidate, g_currentAlgorithm);
	}

	return anyReductions;
}

////////////////////////////////////////////////////////////////////////////////

typedef struct {
	CellSet*		m_cellSet;
	IntList			m_locations;
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
		IntList locations = cellSet->getLocationsForCandidate(candidate);

		TRACE(3, "%s(this=%s, candidate=%d) trying %s: locations=%s\n",
			__CLASSFUNCTION__, m_name.c_str(), candidate+1, cellSet->getName().c_str(),
			locations.toString());

		// If this CellSet has candidate in exactly "n" locations, then save that info
		if (locations.getLength() == n) {
			XWingInfo* info = &xWingInfo[numCellSetsWithCandidateInNLocations++];
			info->m_cellSet = cellSet;
			info->m_locations = locations; // TBD: remove this "extra" copy!
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

			if (infoToMatch->m_locations == info->m_locations) {
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

				bool status = cellSet->checkForXWingReductions(candidate, infoToMatch->m_locations);
				if (status) {
					TRACE(2, "%s(this=%s, n=%d, candidate=%d) locations=%s\n",
						__CLASSFUNCTION__, m_name.c_str(), n, candidate+1,
						infoToMatch->m_locations.toString());

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
	if (level == 0) {
		return;
	}

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

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		anyReductions |= cellSet->checkForNakedSubsets(n);
	}

	return anyReductions;
}

bool CellSetCollection::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(this=%s, n=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), n);

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		anyReductions |= cellSet->checkForHiddenSubsets(n);
	}

	return anyReductions;
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

	bool anyReductions = false;

	for (int i=0; i<ArraySize(m_cells)-2; i++) {
		anyReductions |= m_cells[i]->checkForYWings();
	}

	return anyReductions;
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
		TRACE(0, "Error: file not big enough (%ld)\n", readLen);
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

	bool anyReductions = false;

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		anyReductions |= cellSetCollection->checkForNakedSubsets(n);
	}

	return anyReductions;
}

bool SudokuSolver::checkForHiddenSubsets (int n) {
	TRACE(3, "%s(n=%d)\n", __CLASSFUNCTION__, n);

	bool anyReductions = false;

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		anyReductions |= cellSetCollection->checkForHiddenSubsets(n);
	}

	return anyReductions;
}

bool SudokuSolver::checkForLockedCandidates () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyReductions = false;

	ForEachInCellSetCollectionArray(m_cellSetCollections, cellSetCollection) {
		anyReductions |= cellSetCollection->checkForLockedCandidates();
	}

	return anyReductions;
}

bool SudokuSolver::checkForXWings (int n) {
	TRACE(3, "%s(n=%d)\n", __CLASSFUNCTION__, n);

	bool anyReductions = false;

	for (int candidate=0; candidate<g_N; candidate++) {
		anyReductions |= m_allRows.checkForXWings(n, candidate);
		anyReductions |= m_allCols.checkForXWings(n, candidate);
	}

	return anyReductions;
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
		if (cell->isPossible(candidate)) {
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
	if (!isPossible(candidate)) {
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
	TRACE(3, "%s(candidate=%d, chainStatus=%s)\n",
		__CLASSFUNCTION__, candidate+1, chainStatusToString(chainStatus));

	bool anyReductions = false;

	for (int row=0; row<g_N; row++) {
		for (int col=0; col<g_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			if (cell->getChainStatus() != chainStatus) {
				continue;
			}

			TRACE(3, "%s(candidate=%d, chainStatus=%s) checking %s\n",
				__CLASSFUNCTION__, candidate+1, chainStatusToString(chainStatus), cell->getName().c_str());

			anyReductions |= cell->tryToReduce(candidate, g_currentAlgorithm);
		}
	}

	return anyReductions;
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

	bool anyReductions = false;

	for (int row=0; row<g_N; row++) {
		for (int col=0; col<g_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			if (cell->getChainStatus() != CHAIN_STATUS_UNCOLORED) {
				continue;
			}

			// If this uncolored cell can see two cells with different colors, then it can't be "candidate"
			if (cell->canSee(CHAIN_STATUS_COLOR_RED) && cell->canSee(CHAIN_STATUS_COLOR_BLACK)) {
				anyReductions |= cell->tryToReduce(candidate, g_currentAlgorithm);
			}
		}
	}

	return anyReductions;
}

bool SudokuSolver::checkForSinglesChains (int candidate) {
	TRACE(3, "%s(candidate=%d)\n", __CLASSFUNCTION__, candidate+1);

	m_allCells.resetChains();

	bool anyReductions = false;

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
					if (g_debugLevel > 1) {
						printColors();
					}
					anyReductions = true;
				}

				m_allCells.resetChains();
			}
		}
	}

	return anyReductions;
}

bool SudokuSolver::checkForSinglesChains () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyReductions = false;

	for (int candidate=0; candidate<g_N; candidate++) {
		anyReductions |= checkForSinglesChains(candidate);
	}

	return anyReductions;
}

bool Cell::checkForXYZWings (Cell* cell2) {
	TRACE(3, "%s(this=%s, cell2=%s)\n", __CLASSFUNCTION__, m_name.c_str(), cell2->getName().c_str());

	Cell* cell1 = this;

	IntList* cell1PossibleValues = cell1->getPossibleValuesList();
	IntList* cell2PossibleValues = cell2->getPossibleValuesList();

	if (cell2PossibleValues->getLength() != 2) {
		return false;
	}

	// Make sure the two values of the second cell match two of the three values of the first cell
	IntList intersection12 = IntList::findIntersection(*cell1PossibleValues, *cell2PossibleValues);
	if (intersection12.getLength() != 2) {
		return false;
	}

	bool anyReductions = false;

	// Try to find a third cell that:
	// 1) is visible by the cell1,
	// 2) not visible by the cell2,
	// 3) has two possible values, both in common with cell1 and one in common with cell2
	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, cell3) {
			if ((cell3 == cell1) || (cell3 == cell2)) {
				continue;
			}

			IntList* cell3PossibleValues = cell3->getPossibleValuesList();

			TRACE(3, "%s(this=%s(%s)) cell2=%s(%s), cell3=%s(%s)\n", __CLASSFUNCTION__,
				cell1->getName().c_str(), cell1PossibleValues->toString(),
				cell2->getName().c_str(), cell2PossibleValues->toString(),
				cell3->getName().c_str(), cell3PossibleValues->toString());

			// Make sure cell2 can't see cell3
			if (cell2->hasNeighbor(cell3)) {
				continue;
			}

			if (cell3PossibleValues->getLength() != 2) {
				continue;
			}

			// Make sure the two values of the third cell match two of the three values of the first cell
			IntList intersection13 = IntList::findIntersection(*cell1PossibleValues, *cell3PossibleValues);
			if (intersection13.getLength() != 2) {
				continue;
			}

			// intersection12 and interesection13 CAN'T be the same
			IntList intersection12_13 = IntList::findIntersection(intersection12, intersection13);
			if (intersection12_13.getLength() != 1) {
				continue;
			}

			int candidate = intersection12_13.getValue(0);

			anyReductions |= checkForXYZReductions(candidate, cell2, cell3);		
		}
	}

	return anyReductions;
}

// return true if "candidate" is possible for any neighboring cells of all 3 cells (that aren't any of those cells)
bool Cell::checkForXYZReductions (int candidate, Cell* cell2, Cell* cell3) {
	Cell* cell1 = this;

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, cell) {
			if ((cell == cell1) || (cell == cell2) || (cell == cell3)) {
				continue;
			}

			if (!cell2->hasNeighbor(cell) || !cell3->hasNeighbor(cell)) {
				continue;
			}

			anyReductions |= cell->tryToReduce(candidate, g_currentAlgorithm);
		}
	}

	return anyReductions;
}

bool Cell::checkForXYZWings () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	IntList* possibleValuesList = m_possibleValues.getList();

	// Make sure this cell has 3 possible values
	if (possibleValuesList->size() != 3) {
		return false;
	}

	TRACE(3, "%s(this=%s) has 3 possible values: %s\n",
		__CLASSFUNCTION__, m_name.c_str(), possibleValuesList->toString());

	bool anyReductions = false;

	ForEachInCellSetArray(m_cellSets, cellSet) {
		ForEachInCellArray(cellSet->m_cells, cell2) {
			if (cell2 == this) {
				continue;
			}

			anyReductions |= checkForXYZWings(cell2);
		}
	}

	return anyReductions;
}

bool AllCells::checkForXYZWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	bool anyReductions = false;

	ForEachInCellArray(m_cells, cell) {
		anyReductions |= cell->checkForXYZWings();
	}

	return anyReductions;
}

bool SudokuSolver::checkForXYZWings () {
	TRACE(3, "%s()\n", __CLASSFUNCTION__);

	return m_allCells.checkForXYZWings();
}

bool SudokuSolver::runAlgorithm (AlgorithmType algorithm) {
	TRACE(3, "%s(algorithm=%s)\n", __CLASSFUNCTION__, algorithmToString(algorithm));

	g_currentAlgorithm = algorithm;

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
