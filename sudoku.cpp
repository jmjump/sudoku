#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "Common.h"
#include "CLI.h"

////////////////////////////////////////////////////////////////////////////////

class Cell;
typedef std::vector<Cell*>				CellVector;
typedef CellVector::iterator			CellVectorIterator;

class CellSet;
typedef std::vector<CellSet*>			CellSetVector;
typedef CellSetVector::iterator			CellSetVectorIterator;

typedef enum {
	ALG_CHECK_FOR_NAKED_SINGLES,
	ALG_CHECK_FOR_HIDDEN_SINGLES,
	ALG_CHECK_FOR_PAIRS,
	ALG_CHECK_FOR_LOCKED_CANDIDATES,

	NUM_ALGORITHMS
} AlgorithmType;

typedef enum {
	ROW_COLLECTION,
	COL_COLLECTION,
	BOX_COLLECTION,

	NUM_COLLECTIONS
} CollectionType;

////////////////////////////////////////////////////////////////////////////////

class Cell {
	public:
										Cell (int N, int row, int col) {
											m_N = N;
											m_row = row;
											m_col = col;

											m_name = makeString("(R%dC%d)", m_row+1, m_col+1);

											m_possibles = (bool*) malloc(m_N * sizeof(bool));

											for (int i=0; i<NUM_COLLECTIONS; i++) {
												m_cellSets[i] = NULL;
											}

											reset();
										}

										~Cell () {
											free(m_possibles);
										}

		void							reset () {
											m_known = false;

											for (int i=0; i<m_N; i++) {
												m_possibles[i] = true;
											}
										}

		void							setValue (int value);

		void							setTaken (int value) {
											if (!m_known) {
												m_possibles[value] = false;
											}
										}

		void							setCellSet (CellSet* cellSet) {
											for (int i=0; i<NUM_COLLECTIONS; i++) {
												if (m_cellSets[i] == NULL) {
													m_cellSets[i] = cellSet;
													break;
												}
											}
										}

		bool							getKnown () {
											return m_known;
										}

		int								getValue () {
											return m_value;
										}

		bool							checkForNakedSingles ();
		int								getNumPossibleValues ();
		bool							haveSamePossibles (Cell* otherCell);
		bool							haveAnyOverlappingPossibles (Cell* otherCell);
		bool							tryToReduce (Cell* firstCell, Cell* secondCell);

										// return true if this cell could have "i" for a value. Otherwise, false
		bool							getPossible (int i) {
											return (!m_known && m_possibles[i]);
										}

		bool							tryToReduce (int i);

		int								getRow () { return m_row; }
		int								getCol () { return m_col; }

		CellSet*						getCellSet (CollectionType collection) { return m_cellSets[collection]; }

		std::string						getName () { return m_name; }

	protected:
		std::string						m_name;
		int								m_N;
		int								m_row;
		int								m_col;
		bool							m_known;
		int								m_value;

		bool*							m_possibles;

		// This cell is part of 3 collections:
		// 1) a row
		// 2) a column
		// 3) a box
		// m_cellSets provides a back pointer to those structures
		CellSet*						m_cellSets[NUM_COLLECTIONS];
};

////////////////////////////////////////////////////////////////////////////////

class CellSet {
	public:
										CellSet (std::string name, int n) {
											m_name = name;

											m_n = n;
											m_N = n * n;
											m_cells.resize(m_N);
										}

										~CellSet () {
										}

		void							setCell (int i, Cell* cell) {
											m_cells[i] = cell;

											cell->setCellSet(this);
										}

		Cell*							getCell (int i) {
											return m_cells[i];
										}

		void							setTaken (int value) {
											for (int i=0; i<m_N; i++) {
												Cell* cell = getCell(i);
												cell->setTaken(value);
											}
										}

		int								getNumUnknownCells();

		bool							checkForNakedSingles ();

		bool							checkForHiddenSingles ();
		bool							checkForHiddenSingles (int c);

		bool							checkForPairs ();
		bool							checkForPairs (Cell*);

		void							getBoxCells (Cell* boxCells[], bool isRow, int i);
		bool							cellInSet (Cell* cell, Cell* cellSet[]);
		bool							checkForLockedCandidates ();
		bool							checkForLockedCandidate (int c);
		bool							checkForLockedCandidate (int c, int i, bool isRow);
		bool							checkForLock (int c, Cell* boxCells[]);
		bool							lockCandidateRemoval (int c, Cell* boxCells[]);

		bool							runAlgorithm (AlgorithmType algorithm);

		bool							validate ();

	protected:
		std::string						m_name;

		int								m_n;
		int								m_N;
		CellVector						m_cells;
};

////////////////////////////////////////////////////////////////////////////////

class SetOfCellSets {
	public:
										SetOfCellSets (std::string name, int n) {
											m_name = name;
											m_n = n;
											m_N = n * n;

											//m_cellSets = (CellSet**) malloc(m_N * sizeof(CellSet*));
											m_cellSets.resize(m_N);

											for (int i=0; i<m_N; i++) {
												m_cellSets[i] = new CellSet(makeString("%s%d", m_name.c_str(), i+1), m_n);
											}
										}

										virtual ~SetOfCellSets () {
											for (int i=0; i<m_N; i++) {
												delete m_cellSets[i];
											}

											//free(m_cellSets);
										}

		void							setCell (int row, int col, Cell* cell) {
											int index1 = getIndex1(row, col);
											int index2 = getIndex2(row, col);
		
											CellSet* cellSet = m_cellSets[index1];
											cellSet->setCell(index2, cell);
										}

		CellSet*						getCellSet (int i) {
											return m_cellSets[i];
										}

		bool							runAlgorithm (AlgorithmType algorithm);

		bool							validate ();

	protected:
		virtual int						getIndex1 (int row, int col) = 0;
		virtual int						getIndex2 (int row, int col) = 0;

		std::string						m_name;
		int								m_n;
		int								m_N;
		CellSetVector					m_cellSets;
};

////////////////////////////////////////////////////////////////////////////////

class SetOfRows : public SetOfCellSets {
	public:
										SetOfRows (int n) : SetOfCellSets("row", n) { }

		int								getIndex1 (int row, int col) { return row; }
		int								getIndex2 (int row, int col) { return col; }
};

////////////////////////////////////////////////////////////////////////////////

class SetOfCols : public SetOfCellSets {
	public:
										SetOfCols (int n) : SetOfCellSets("col", n) { }

		int								getIndex1 (int row, int col) { return col; }
		int								getIndex2 (int row, int col) { return row; }
};

////////////////////////////////////////////////////////////////////////////////

class SetOfBoxes : public SetOfCellSets {
	public:
										SetOfBoxes (int n) : SetOfCellSets("box", n) { }

		int								getIndex1 (int row, int col) {
											int rowBase = row / m_n;
											int colBase = col / m_n;

											int index = (rowBase * m_n) + colBase;

											return index;
										}

		int								getIndex2 (int row, int col) {
											int rowBase = row % m_n;
											int colBase = col % m_n;

											int index = (rowBase * m_n) + colBase;

											return index;
										}
};

////////////////////////////////////////////////////////////////////////////////

class AllCells {
	public:
										AllCells (int n) {
											m_n = n;
											m_N = n * n;
											m_cells = (Cell**) malloc(m_N * m_N * sizeof(Cell*));

											for (int row=0; row<m_N; row++) {
												for (int col=0; col<m_N; col++) {
													Cell* cell = new Cell(m_N, row, col);

													int index = getIndex(row, col);
													m_cells[index] = cell;
												}
											}
										}

										~AllCells () {
											free(m_cells);
										}

		Cell*							getCell (int row, int col) {
											int index = getIndex(row, col);

											return m_cells[index];
										}

		void							setValue (int row, int col, int value) {
											Cell* cell = getCell(row, col);
											cell->setValue(value);
										}

		void							reset () {
											for (int row=0; row<m_N; row++) {
												for (int col=0; col<m_N; col++) {
													Cell* cell = getCell(row, col);
													cell->reset();
												}
											}
										}

		bool							isSolved () {
											for (int row=0; row<m_N; row++) {
												for (int col=0; col<m_N; col++) {
													Cell* cell = getCell(row, col);

													if (!cell->getKnown()) {
														return false;
													}
												}
											}

											return true;
										}

	protected:
		int								getIndex (int row, int col) {
											return (row * m_N) + col;
										}

		int								m_n;
		int								m_N;
		Cell**							m_cells;
};

////////////////////////////////////////////////////////////////////////////////

class SudokuSolver {
	public:
						SudokuSolver (int n) : m_n(n), m_N(n*n), m_allCells(n), m_allRows(n), m_allCols(n), m_allBoxes(n) {
							m_allCellSets[ROW_COLLECTION] = &m_allRows;
							m_allCellSets[COL_COLLECTION] = &m_allCols;
							m_allCellSets[BOX_COLLECTION] = &m_allBoxes;

							// initialize the collections
							for (int row=0; row<m_N; row++) {
								for (int col=0; col<m_N; col++) {
									Cell* cell = m_allCells.getCell(row, col);

									for (int i=0; i<NUM_COLLECTIONS; i++) {
										m_allCellSets[i]->setCell(row, col, cell);
									}
								}
							}
						}

						~SudokuSolver () {
						}

		void			init (const char* filename);
		void			solve ();
		bool			tryToSolve ();
		bool			runAlgorithm (AlgorithmType algorithm);

		void			reset ();
		void			print (int level=0);

		bool			isSolved () {
							return m_allCells.isSolved();
						}

		bool			validate ();

	protected:
		int				m_n;
		int				m_N;

		AllCells		m_allCells;

		SetOfRows		m_allRows;
		SetOfCols		m_allCols;
		SetOfBoxes		m_allBoxes;

		SetOfCellSets*	m_allCellSets[NUM_COLLECTIONS];
};

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

bool Cell::checkForNakedSingles () {
	TRACE(2, "%s(this=%s)\n",
		__CLASSFUNCTION__, m_name.c_str());

	if (m_known) {
		TRACE(3, "    already known (%d)\n", m_value+1);
	} else {
		int onlyValue = -1;

		for (int i=0; i<m_N; i++) {
			TRACE(3, "    %d: %s BE\n", i+1, m_possibles[i] ? "CAN" : "CAN'T");

			if (m_possibles[i]) {
				if (onlyValue != -1) {
					return false;
				} else {
					onlyValue = i;
				}
			}
		}

		if (onlyValue != -1) {
			TRACE(1, "%s(this=%s) can only be %d\n",
				__CLASSFUNCTION__, m_name.c_str(), onlyValue+1);

			setValue(onlyValue);

			return true;
		}
	}

	return false;
}

int Cell::getNumPossibleValues () {
	int numPossibleValues = 0;

	for (int i=0; i<m_N; i++) {
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

	for (int i=0; i<m_N; i++) {
		if (otherCell->m_possibles[i] && !m_possibles[i]) {
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

	for (int i=0; i<m_N; i++) {
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

	for (int i=0; i<m_N; i++) {
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
	if (getPossible(c)) {
		TRACE(3, "%s(this=%s, candidate=%d) no longer possible\n",
			__CLASSFUNCTION__, m_name.c_str(), c+1);

		m_possibles[c] = false;
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CellSet::checkForNakedSingles () {
	TRACE(2, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyChanges = false;

	for (CellVectorIterator it=m_cells.begin(); it != m_cells.end(); it++) {
		Cell* cell = *it;

		anyChanges |= cell->checkForNakedSingles();
	}

	TRACE(2, "%s() return %s\n",
		__CLASSFUNCTION__, anyChanges ? "TRUE" : "FALSE");

	return anyChanges;
}

bool CellSet::checkForHiddenSingles (int c) {
	TRACE(2, "%s(this=%s, c=%d)\n", __CLASSFUNCTION__, m_name.c_str(), c+1);

	int winningPosition = -1;
	for (int position=0; position<m_N; position++) {
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

	for (int i=0; i<m_N; i++) {
		anyChanges |= checkForHiddenSingles(i);
	}

	return anyChanges;
}

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

	// How many unknown cells are there?
	int numUnknownCells = getNumUnknownCells();
	if (numUnknownCells < 3) { // TBD: function of the number we're looking for
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

void CellSet::getBoxCells (Cell* boxCells[], bool isRow, int i) {
	int pos0, pos1, pos2;

	if (isRow) {
		pos0 = i * m_n;
		pos1 = pos0 + 1;
		pos2 = pos1 + 1;
	} else {
		pos0 = i;
		pos1 = pos0 + m_n;
		pos2 = pos1 + m_n;
	}

	boxCells[0] = getCell(pos0);
	boxCells[1] = getCell(pos1);
	boxCells[2] = getCell(pos2);
}

// return "true" if the specified cell is in the specified set of cells. Otherwise, return false
bool CellSet::cellInSet (Cell* cell, Cell* cellSet[]) {
	for (int i=0; i<m_n; i++) {
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

	for (int pos=0; pos<m_N; pos++) {
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

bool CellSet::lockCandidateRemoval (int c, Cell* boxCells[]) {
	TRACE(3, "%s(this=%s, candidate=%d)\n",
		__CLASSFUNCTION__, m_name.c_str(), c+1);

	bool anyReductions = false;

	for (int i=0; i<m_N; i++) {
		Cell* cell = getCell(i);

		if (cell->getKnown()) {
			continue;
		}

		if (cellInSet(cell, boxCells)) {
			continue;
		}

		bool status = cell->tryToReduce(c);
		if (status) {
			TRACE(1, "%s(this=%s, candidate=%d) no longer possible\n",
				__CLASSFUNCTION__, m_name.c_str(), c+1);

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

	for (int i=0; i<m_n; i++) {
		anyReductions |= checkForLockedCandidate(c, i, true);
		anyReductions |= checkForLockedCandidate(c, i, false);
	}

	return anyReductions;
}

bool CellSet::checkForLockedCandidates () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool anyReductions = false;

	for (int c=0; c<m_N; c++) {
		anyReductions |= checkForLockedCandidate(c);
	}

	return anyReductions;
}

bool CellSet::runAlgorithm (AlgorithmType algorithm) {
	TRACE(2, "%s(this=%s, algorithm=%d)\n", __CLASSFUNCTION__, m_name.c_str(), algorithm);

	switch (algorithm) {
		case ALG_CHECK_FOR_NAKED_SINGLES:
			return checkForNakedSingles();

		case ALG_CHECK_FOR_HIDDEN_SINGLES:
			return checkForHiddenSingles();

		case ALG_CHECK_FOR_PAIRS:
			return checkForPairs();

		case ALG_CHECK_FOR_LOCKED_CANDIDATES:
			return checkForLockedCandidates();

		default:
			break;
	}

	return false;
}

bool CellSet::validate () {
	TRACE(3, "%s(this=%s)\n", __CLASSFUNCTION__, m_name.c_str());

	bool valid = true;

	for (int i=0; i<m_N; i++) {
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

////////////////////////////////////////////////////////////////////////////////

bool SetOfCellSets::runAlgorithm (AlgorithmType algorithm) {
	TRACE(2, "%s(this=%s, algorithm=%d)\n", __CLASSFUNCTION__, m_name.c_str(), algorithm);

	bool anyChanges = false;

	for (CellSetVectorIterator it=m_cellSets.begin(); it != m_cellSets.end(); it++) {
		CellSet* cellSet = *it;

		anyChanges |= cellSet->runAlgorithm(algorithm);
	}

	return anyChanges;
}

bool SetOfCellSets::validate () {
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

	reset ();

	char buffer[100]; // TBD: make variable
	for (int row=0; row<m_N; ) {
		if (!fgets(buffer, sizeof(buffer), fp)) {
			break;
		}
		if (strlen(buffer) < m_N) {
			continue;
		}

		int col=0;
		for (int i=0; col<m_N; i++) {
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
	m_allCells.reset();
}

bool SudokuSolver::runAlgorithm (AlgorithmType algorithm) {
	TRACE(2, "%s(algorithm=%d)\n", __CLASSFUNCTION__, algorithm);

	bool anyChanges = false;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		// short circuits!
		// TBD: this is gross! But, at least it's only in one place
		if (algorithm == ALG_CHECK_FOR_NAKED_SINGLES) {
			if (collection != ROW_COLLECTION) {
				continue;
			}
		} else if (algorithm == ALG_CHECK_FOR_LOCKED_CANDIDATES) {
			if (collection != BOX_COLLECTION) {
				continue;
			}
		}

		SetOfCellSets* setOfCellSets = m_allCellSets[collection];

		anyChanges |= setOfCellSets->runAlgorithm(algorithm);
	}

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
		print();

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
	static char buffer[2000];
	char* p = buffer;

	for (int row=0; row<m_N; row++) {
		for (int col=0; col<m_N; col++) {
			Cell* cell = m_allCells.getCell(row, col);

			bool known = cell->getKnown();
			if (known) {
				int value = cell->getValue();
				p += sprintf(p, "%d", value+1);
			} else {
				p += sprintf(p, "-");
			}

			if ((col % m_n) == (m_n - 1)) {
				p += sprintf(p, " ");
			}
		}

		// more detail!
		if (level > 0) {
			for (int col=0; col<m_N; col++) {
				p += sprintf(p, "[");

				Cell* cell = m_allCells.getCell(row, col);

				bool known = cell->getKnown();
				if (known) {
					p += sprintf(p, "         ");
				} else {
					for (int i=0; i<m_N; i++) {
						if (cell->getPossible(i)) {
							p += sprintf(p, "%d", i+1);
						} else {
							p += sprintf(p, "-");
						}
					}
				}

				p += sprintf(p, "]");

				if ((col % m_n) == (m_n - 1)) {
					p += sprintf(p, "   ");
				}
			}
		}

		p += sprintf(p, "\n");

		if ((row % m_n) == (m_n - 1)) {
			p += sprintf(p, "\n");
		}
	}

	TRACE(0, "%s()\n%s", __CLASSFUNCTION__, buffer);
}

bool SudokuSolver::validate () {
	bool valid = true;

	for (int collection=0; collection<NUM_COLLECTIONS; collection++) {
		SetOfCellSets* setOfCellSets = m_allCellSets[collection];

		valid &= setOfCellSets->validate();
	}

	TRACE((valid ? 2 : 0), "%s() status=%s\n", __CLASSFUNCTION__, valid ? "valid" : "INVALID");

	return valid;
}

////////////////////////////////////////////////////////////////////////////////

static void printHelp (const char* argv0) {
	printf("%s [OPTIONS] <filename> [<filename> ...]\n", argv0);

	exit(0);
}

static void printVersion (const char* argv0) {
	static const char* version = "0.1";

	printf("%s version %s (%s %s)\n", argv0, version, __DATE__, __TIME__);

	exit(0);
}

SudokuSolver* g_solver;

////////////////////////////////////////////////////////////////////////////////

static void processGame (CLI* cli) {
	char* filename = cli->getStringParameter();
	if (!filename) {
		return;
	}

	g_solver->init(filename);
}

static void processPrint (CLI* cli) {
	int level = cli->getIntParameter(false, 0);

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

////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
	int opt;
    while ((opt = getopt(argc, argv, "hvdD:")) != EOF) {
        if (opt == 'h') {
            printHelp(argv[0]);
        } else if (opt == 'v') {
            printVersion(argv[0]);
        } else if (opt == 'd') {
            g_debugLevel++;
        } else if (opt == 'D') {
            g_debugLevel = atoi(optarg);
        }
    }

	int n = 3;
	g_solver = new SudokuSolver(n);

	if (optind < argc) {
        const char* filename = argv[optind];
		g_solver->init(filename);
		g_solver->print();
	}

	CLI cli;

	cli.addCommand("game", processGame, "<filename> : load a game from the specified file");
	cli.addCommand("print", processPrint, "[<detail level>] : print the game board");
	cli.addCommand("step", processStep, "[<numSteps>] : run the algorithm numSteps times (default=1)");
	cli.addCommand("run", processRun, ": run the algorithm to completion");

	cli.processInput(stdin);
}
