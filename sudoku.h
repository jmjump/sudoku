#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "Common.h"

////////////////////////////////////////////////////////////////////////////////

#define g_n								3 // Boxes are 3x3
#define g_N								(g_n * g_n)	// 9 cells in a row, col or box

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
	ALG_CHECK_FOR_YWINGS,

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
										Cell (int row, int col) {
											m_row = row;
											m_col = col;
											m_box = (row / g_n) + (col / g_n);

											m_name = makeString("R%dC%d", m_row+1, m_col+1);

											for (int i=0; i<NUM_COLLECTIONS; i++) {
												m_cellSets[i] = NULL;
											}

											reset();
										}

										~Cell () {
										}

		void							reset () {
											m_known = false;

											for (int i=0; i<g_N; i++) {
												m_possibles[i] = true;
											}
										}

		void							setValue (int value);

		void							setTaken (int value) {
											if (!m_known) {
												m_possibles[value] = false;
											}
										}

		void							setCellSet (CollectionType collection, CellSet* cellSet) {
											m_cellSets[collection] = cellSet;
										}

		bool							getKnown () {
											return m_known;
										}

		int								getValue () {
											return m_value;
										}

		bool							hasNeighbor (Cell* otherCell);
		bool							haveExactPossibles (Cell* otherCell);
		void							processNakedSingle ();
		int								getPossibleValues (int possibles[]=NULL);
		bool							haveSamePossibles (Cell* otherCell);
		bool							haveAnyOverlappingPossibles (Cell* otherCell);
		bool							checkForNakedReductions (Cell* nakedCell);
		bool							areAnyOfTheseValuesUntaken (int n, int values[]);
		bool							hiddenSubsetReduction (int n, int values[]);
		bool							hiddenSubsetReduction2 (int n, Cell* candidateCells[], int values[]);
		bool							checkForYWings ();
		bool							checkForYWings (Cell* secondCell);
		bool							checkForYWingReductions (int c, Cell* otherCell);

										// return true if this cell could have "i" for a value. Otherwise, false
		bool							getPossible (int i) {
											return (!m_known && m_possibles[i]);
										}

		bool							tryToReduce (int c);

										// Get the row/col/box
		int								getRCB (int rcb) {
											return
												(rcb == ROW_COLLECTION) ? m_row :
												(rcb == COL_COLLECTION) ? m_col :
												m_box;
										}
		int								getRow () { return m_row; }
		int								getCol () { return m_col; }
		int								getBox () { return m_box; }

		CellSet*						getCellSet (CollectionType collection) { return m_cellSets[collection]; }

		std::string						getName () { return m_name; }

	protected:
		std::string						m_name;
		int								m_row;
		int								m_col;
		int								m_box;
		bool							m_known;
		int								m_value;

		bool							m_possibles[g_N];

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
										CellSet (CollectionType collection, std::string name);
										~CellSet () {
										}

		void							reset ();

		void							setCell (int i, Cell* cell) {
											m_cells[i] = cell;

											cell->setCellSet(m_collection, this);
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
		bool							checkForLockedCandidate2 (int candidate, CollectionType collection, int numLocations, int locations[]);
		bool							inSameRCB (CollectionType collection, int numLocations, int locations[]);
		bool							lockedCandidateReduction (int candidate, int numCells, Cell* cells[]);
		int								findMatchingCells (Cell* cellToMatch, Cell* matchingCells[]);
		bool							checkForXWingReductions (int candidate, int numLocations, int locations[]);

		bool							tryToReduce (int N, Cell* matchingCells[]);

		bool							runAlgorithm (AlgorithmType algorithm);
		bool							checkForNakedSubsets (int n);
		bool							checkForHiddenSubsets (int n);
		bool							checkForHiddenSubsets (int n, int permutation[]);
		bool							hiddenSubsetReduction (int n, int permutation[]);
		bool							hiddenSubsetReduction2 (int n, Cell* candidateCells[], int values[]);
		int								getLocationsForCandidate (int candidate, int locations[]);

		bool							hasCandidateCell (Cell* candidateCell);
		bool							hasAllCandidateCells (int n, Cell* candidateCells[]);

		bool							validate (int level=0);

		std::string						getName () { return m_name; }

		std::string						toString (int level);

	protected:
		CollectionType					m_collection;
		std::string						m_name;

		CellVector						m_cells;
		bool							m_isTaken[g_N];
};

////////////////////////////////////////////////////////////////////////////////

class AllCellSets {
	public:
										AllCellSets (CollectionType type, std::string name) {
											m_type = type;
											m_name = makeString("All%ss", name.c_str());

											m_cellSets.resize(g_N);

											for (int i=0; i<g_N; i++) {
												m_cellSets[i] = new CellSet(m_type, makeString("%s%d", name.c_str(), i+1));
											}
										}

										virtual ~AllCellSets () {
											for (int i=0; i<g_N; i++) {
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

		bool							checkForLockedCandidates ();
		bool							checkForNakedSubsets (int n);
		bool							checkForHiddenSubsets (int n);
		virtual bool					checkForXWings ();
		bool							checkForXWings (int candidate);

		bool							validate (int level=0);

		void							print (int level);

	protected:
		virtual int						getIndex1 (int row, int col) = 0;
		virtual int						getIndex2 (int row, int col) = 0;

		CollectionType					m_type;

		std::string						m_name;
		CellSetVector					m_cellSets;
};

////////////////////////////////////////////////////////////////////////////////

class AllRows : public AllCellSets {
	public:
										AllRows (int n) : AllCellSets(ROW_COLLECTION, "Row") { }

		int								getIndex1 (int row, int col) { return row; }
		int								getIndex2 (int row, int col) { return col; }
};

////////////////////////////////////////////////////////////////////////////////

class AllCols : public AllCellSets {
	public:
										AllCols (int n) : AllCellSets(COL_COLLECTION, "Col") { }

		int								getIndex1 (int row, int col) { return col; }
		int								getIndex2 (int row, int col) { return row; }
};

////////////////////////////////////////////////////////////////////////////////

class AllBoxes : public AllCellSets {
	public:
										AllBoxes (int n) : AllCellSets(BOX_COLLECTION, "Box") { }

		int								getIndex1 (int row, int col) {
											int rowBase = row / g_n;
											int colBase = col / g_n;

											int index = (rowBase * g_n) + colBase;

											return index;
										}

		int								getIndex2 (int row, int col) {
											int rowBase = row % g_n;
											int colBase = col % g_n;

											int index = (rowBase * g_n) + colBase;

											return index;
										}

		bool							checkForXWings () { return false; }
};

////////////////////////////////////////////////////////////////////////////////

class AllCells {
	public:
										AllCells (int n) {
											for (int row=0; row<g_N; row++) {
												for (int col=0; col<g_N; col++) {
													Cell* cell = new Cell(row, col);

													int index = getIndex(row, col);
													m_cells[index] = cell;
												}
											}
										}

										~AllCells () {
										}

		Cell*							getCell (int row, int col) {
											int index = getIndex(row, col);

											return m_cells[index];
										}

		void							setValue (int row, int col, int value) {
											Cell* cell = getCell(row, col);
											cell->setValue(value);
										}

		bool							isSolved () {
											for (int row=0; row<g_N; row++) {
												for (int col=0; col<g_N; col++) {
													Cell* cell = getCell(row, col);

													if (!cell->getKnown()) {
														return false;
													}
												}
											}

											return true;
										}

		bool							checkForYWings ();

	protected:
		int								getIndex (int row, int col) {
											return (row * g_N) + col;
										}

		Cell*							m_cells[g_N * g_N];
};

////////////////////////////////////////////////////////////////////////////////

class SudokuSolver {
	public:
										SudokuSolver () : m_allCells(g_n), m_allRows(g_n), m_allCols(g_n), m_allBoxes(g_n) {
											m_allCellSets[ROW_COLLECTION] = &m_allRows;
											m_allCellSets[COL_COLLECTION] = &m_allCols;
											m_allCellSets[BOX_COLLECTION] = &m_allBoxes;

											// initialize the collections
											for (int row=0; row<g_N; row++) {
												for (int col=0; col<g_N; col++) {
													Cell* cell = m_allCells.getCell(row, col);

													for (int i=0; i<NUM_COLLECTIONS; i++) {
														m_allCellSets[i]->setCell(row, col, cell);
													}
												}
											}
										}

										~SudokuSolver () {
										}

		void							init (const char* filename);
		void							solve ();
		bool							tryToSolve ();
		bool							runAlgorithm (AlgorithmType algorithm);
		bool							checkForNakedSubsets (int n);
		bool							checkForHiddenSubsets (int n);
		bool							checkForLockedCandidates ();
		bool							checkForXWings ();
		bool							checkForYWings ();

		void							reset ();
		void							print (int level=0);

		bool							isSolved () {
											return m_allCells.isSolved();
										}

		bool							validate (int level=0);

		void							listAlgorithms ();

	protected:
		AllCells						m_allCells;

		AllRows							m_allRows;
		AllCols							m_allCols;
		AllBoxes						m_allBoxes;

		AllCellSets*					m_allCellSets[NUM_COLLECTIONS];
};
