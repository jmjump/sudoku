#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "Common.h"

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
	ALG_CHECK_FOR_NAKED_PAIRS,
	ALG_CHECK_FOR_HIDDEN_PAIRS,
	ALG_CHECK_FOR_LOCKED_CANDIDATES,
	ALG_CHECK_FOR_XWINGS,

	NUM_ALGORITHMS
} AlgorithmType;

extern const char* toString (AlgorithmType);

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

											m_name = makeString("R%dC%d", m_row+1, m_col+1);

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

		bool							haveExactPossibles (Cell* otherCell);
		void							processNakedSingle ();
		int								getNumPossibleValues ();
		bool							haveSamePossibles (Cell* otherCell);
		bool							haveAnyOverlappingPossibles (Cell* otherCell);
		bool							tryToReduce (Cell* firstCell, Cell* secondCell);
		bool							checkForNakedReductions (Cell* nakedCell);
		bool							areAnyOfTheseValuesUntaken (int n, int values[]);
		bool							hiddenSubsetReduction (int n, int values[]);

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
										CellSet (CollectionType type, std::string name, int n);
										~CellSet () {
										}

		void							reset ();

		void							setCell (int i, Cell* cell) {
											m_cells[i] = cell;

											cell->setCellSet(this);
										}

		Cell*							getCell (int i) {
											return m_cells[i];
										}

		void							setTaken (int value);

		int								getUntakenNumbers(int untakenNumbers[]=NULL);

		void							getBoxCells (Cell* boxCells[], bool isRow, int i);
		bool							cellInSet (Cell* cell, Cell* cellSet[]);
		bool							checkForLockedCandidates ();
		bool							checkForLockedCandidate (int c);
		bool							checkForLockedCandidate (int c, int i, bool isRow);
		bool							checkForLock (int c, Cell* boxCells[]);
		bool							lockCandidateRemoval (int c, Cell* boxCells[]);
		int								findMatchingCells (Cell* cellToMatch, Cell* matchingCells[]);
		bool							checkForXWingReductions (int candidate, int numLocations, int locations[]);

		bool							tryToReduce (int N, Cell* matchingCells[]);

		bool							runAlgorithm (AlgorithmType algorithm);
		bool							checkForNakedSubsets (int n);
		bool							checkForHiddenSubsets (int n);
		bool							checkForHiddenSubsets (int n, int permutation[]);
		bool							hiddenSubsetReduction (int n, int permutation[]);
		int								getLocationsForCandidate (int candidate, int locations[]);

		bool							validate ();

		std::string						getName () { return m_name; }

		std::string						toString (int level);

	protected:
		CollectionType					m_type;
		std::string						m_name;

		int								m_n;
		int								m_N;
		CellVector						m_cells;
		bool							m_isTaken[9]; // TBD:
};

////////////////////////////////////////////////////////////////////////////////

class AllCellSets {
	public:
										AllCellSets (CollectionType type, std::string name, int n) {
											m_type = type;
											m_name = makeString("All%ss", name.c_str());
											m_n = n;
											m_N = n * n;

											m_cellSets.resize(m_N);

											for (int i=0; i<m_N; i++) {
												m_cellSets[i] = new CellSet(m_type, makeString("%s%d", name.c_str(), i+1), m_n);
											}
										}

										virtual ~AllCellSets () {
											for (int i=0; i<m_N; i++) {
												delete m_cellSets[i];
											}
										}

		void							reset ();

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
		virtual bool					checkForLockedCandidates () { return false; }
		virtual bool					checkForXWings ();
		bool							checkForXWings (int candidate);
		bool							checkForNakedSubsets (int n);
		bool							checkForHiddenSubsets (int n);

		bool							validate ();

		void							print (int level);

	protected:
		virtual int						getIndex1 (int row, int col) = 0;
		virtual int						getIndex2 (int row, int col) = 0;

		CollectionType					m_type;

		std::string						m_name;
		int								m_n;
		int								m_N;
		CellSetVector					m_cellSets;
};

////////////////////////////////////////////////////////////////////////////////

class AllRows : public AllCellSets {
	public:
										AllRows (int n) : AllCellSets(ROW_COLLECTION, "Row", n) { }

		int								getIndex1 (int row, int col) { return row; }
		int								getIndex2 (int row, int col) { return col; }
};

////////////////////////////////////////////////////////////////////////////////

class AllCols : public AllCellSets {
	public:
										AllCols (int n) : AllCellSets(COL_COLLECTION, "Col", n) { }

		int								getIndex1 (int row, int col) { return col; }
		int								getIndex2 (int row, int col) { return row; }
};

////////////////////////////////////////////////////////////////////////////////

class AllBoxes : public AllCellSets {
	public:
										AllBoxes (int n) : AllCellSets(BOX_COLLECTION, "Box", n) { }

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

		bool							checkForLockedCandidates ();
		bool							checkForXWings () { return false; }
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

/*
PLEASE DELETE ME
		void							reset () {
											for (int row=0; row<m_N; row++) {
												for (int col=0; col<m_N; col++) {
													Cell* cell = getCell(row, col);
													cell->reset();
												}
											}
										}
*/

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

		AllRows			m_allRows;
		AllCols			m_allCols;
		AllBoxes		m_allBoxes;

		AllCellSets*	m_allCellSets[NUM_COLLECTIONS];
};
