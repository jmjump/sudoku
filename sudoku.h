#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "Common.h"

////////////////////////////////////////////////////////////////////////////////

#define g_n									3 // Boxes are 3x3
#define g_N									(g_n * g_n)	// 9 cells in a row, col or box

class Cell;
typedef std::vector<Cell*>					CellVector;
typedef CellVector::iterator				CellVectorIterator;

class CellSet;
typedef std::vector<CellSet*>				CellSetVector;
typedef CellSetVector::iterator				CellSetVectorIterator;

class CellSetCollection;
typedef std::vector<CellSetCollection*>		CellSetCollectionVector;
typedef CellSetCollectionVector::iterator	CellSetCollectionVectorIterator;

typedef std::vector<int>					IntVector;

typedef enum {
	ALG_CHECK_FOR_NAKED_SINGLES,
	ALG_CHECK_FOR_HIDDEN_SINGLES,
	ALG_CHECK_FOR_NAKED_PAIRS,
	ALG_CHECK_FOR_NAKED_TRIPLES,
	ALG_CHECK_FOR_HIDDEN_PAIRS,
	ALG_CHECK_FOR_HIDDEN_TRIPLES,
	ALG_CHECK_FOR_NAKED_QUADS,
	ALG_CHECK_FOR_HIDDEN_QUADS,
	ALG_CHECK_FOR_LOCKED_CANDIDATES,
	ALG_CHECK_FOR_XWINGS,
	ALG_CHECK_FOR_YWINGS,
	ALG_CHECK_FOR_SINGLES_CHAINS,
	ALG_CHECK_FOR_SWORDFISH,
	ALG_CHECK_FOR_XYZ_WINGS,

	NUM_ALGORITHMS
} AlgorithmType;

extern const char* alogorithmToString (AlgorithmType);

typedef enum {
	ROW_COLLECTION,
	COL_COLLECTION,
	BOX_COLLECTION,

	NUM_COLLECTIONS
} CollectionType;

extern const char* collectionToString (CollectionType);

typedef enum {
	CHAIN_STATUS_UNCOLORED,
	CHAIN_STATUS_COLOR_RED,
	CHAIN_STATUS_COLOR_BLACK
} ChainStatusType;

extern const char* chainStatusToString (ChainStatusType);

////////////////////////////////////////////////////////////////////////////////

class IntList : public IntVector {
	public:
										IntList ();
										IntList (int n, int values[]);

		bool							operator== (IntList& other);

		void							addValue (int value);
		int								getValue (int i);
		int								getLength ();

		bool							onList (int value);

		const char*						toString ();

		static IntList					findIntersection (IntList& listA, IntList& listB);
		void							findUnion (IntList& otherList);
};

////////////////////////////////////////////////////////////////////////////////

class CellList : public CellVector {
	public:
		void							addValue (Cell*);
		Cell*							getValue (int i);
		int								getLength ();

		const char*						toString ();

		bool							onList (Cell* cell);
};

////////////////////////////////////////////////////////////////////////////////

class PossibleValues {
	public:
										PossibleValues ();

		void							reset ();

		void							setValue (int value);
		int								getValue () { return m_value; }
		bool							getKnown () { return m_known; }

		void							setNoLongerPossible (int value);

		bool							isPossible (int value);

		IntList*						getList () { return &m_list; }

		void							adjustList ();

	protected:
		bool							m_known;
		int								m_value;
		bool							m_possibleValues[g_N];

		IntList							m_list;
};

////////////////////////////////////////////////////////////////////////////////

class Cell {
	public:
										Cell (int row, int col);

		void							reset ();
		void							setValue (int value);
		void							setNoLongerPossible (int value);
		bool							isPossible (int value); // return true if this cell could possibly have this value. Otherwise, false

		void							setCellSet (CollectionType collection, CellSet* cellSet) {
											m_cellSets[collection] = cellSet;
										}

		bool							getKnown () { return m_possibleValues.getKnown(); }
		int								getValue () { return m_possibleValues.getValue(); }

		bool							hasNeighbor (Cell* otherCell);
		bool							haveExactPossibles (Cell* otherCell);
		bool							processNakedSingle ();
		IntList*						getPossibleValuesList () { return m_possibleValues.getList(); }
		bool							haveSamePossibles (Cell* otherCell);
		bool							haveAnyOverlappingPossibles (Cell* otherCell);
		bool							areAnyOfTheseValuesPossible (IntList&);
		bool							hiddenSubsetReduction (IntList&);
		bool							hiddenSubsetReduction2 (CellList& candidateCells, IntList&);
		bool							checkForYWings ();
		bool							checkForYWings (Cell* cell2);
		bool							checkForYWingReductions (int c, Cell* otherCell);
		bool							checkForXYZWings ();
		bool							checkForXYZWings (Cell* cell2);
		bool							checkForXYZReductions (int candidate, Cell* cell2, Cell* cell3);

		bool							tryToReduce (IntList& values, AlgorithmType=NUM_ALGORITHMS);
		bool							tryToReduce (int candidate, AlgorithmType=NUM_ALGORITHMS);

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

		// For chain coloring
		void							resetChain ();
		bool							buildChains (int candidate, ChainStatusType, Cell* linkingCell);
		ChainStatusType					getChainStatus () { return m_chainStatus; }
		bool							isConjugatePair (int candidate);
		bool							canSee (ChainStatusType);

	protected:
		void							adjustPossibleValuesList ();

	protected:
		std::string						m_name;
		int								m_row;
		int								m_col;
		int								m_box;

		PossibleValues					m_possibleValues;

		// This cell is part of 3 collections:
		// 1) a row
		// 2) a column
		// 3) a box
		// m_cellSets provides a back pointer to those structures
		CellSet*						m_cellSets[NUM_COLLECTIONS];

		// For chain coloring
		ChainStatusType					m_chainStatus;
};

////////////////////////////////////////////////////////////////////////////////

class CellSet {
	friend class Cell;

	public:
										CellSet (CollectionType collection, std::string name);

		void							reset ();

		void							setCell (int i, Cell* cell) {
											m_cells[i] = cell;

											cell->setCellSet(m_collection, this);
										}

		Cell*							getCell (int i) {
											return m_cells[i];
										}

		void							setNoLongerPossible (int value);
		IntList*						getPossibleValuesList () { return m_possibleValues.getList(); }

		void							getBoxCells (Cell* boxCells[], bool isRow, int i);
		bool							cellInSet (Cell* cell, Cell* cellSet[]);
		bool							checkForLockedCandidates ();
		bool							checkForLockedCandidate (int c);
		bool							checkForLockedCandidate2 (int candidate, CollectionType collection, IntList& locations);
		bool							inSameRCB (CollectionType collection, IntList& locations);
		bool							lockedCandidateReduction (int candidate, CellList& cellList);
		CellList						findCellsWithSamePossibleValues (Cell* cellToMatch);
		bool							checkForXWingReductions (int candidate, IntList& locations);

		bool							runAlgorithm (AlgorithmType algorithm);
		bool							checkForNakedSubsets (int n);
		bool							nakedSubsetReduction (CellList& cellList, IntList& values);
		bool							checkForHiddenSubsets (int n);
		bool							checkForHiddenSubsets (IntList& permutation);
		bool							hiddenSubsetReduction (IntList& permutation);
		bool							hiddenSubsetReduction2 (CellList& candidateCells, IntList& permutation);
		IntList							getLocationsForCandidate (int candidate);

		bool							hasCandidateCell (Cell* candidateCell);
		bool							hasAllCandidateCells (CellList& candidateCells);

		bool							validate (int level=0);

		std::string						getName () { return m_name; }

		std::string						toString (int level);

		// For chain coloring
		void							buildChains (int candidate, ChainStatusType, Cell* linkingCell);
		bool							checkForSinglesChainsReductions (int candidate);
		int								getNumPossible (int candidate);
		bool							checkChainStatus (ChainStatusType);
		int								getChainStatusCount (ChainStatusType);

	protected:
		CollectionType					m_collection;
		std::string						m_name;

		PossibleValues					m_possibleValues;

		Cell*							m_cells[g_N];
};

////////////////////////////////////////////////////////////////////////////////

class CellSetCollection {
	public:
										CellSetCollection (CollectionType type, std::string name) {
											m_type = type;
											m_name = makeString("All%ss", name.c_str());

											for (int i=0; i<g_N; i++) {
												m_cellSets[i] = new CellSet(m_type, makeString("%s%d", name.c_str(), i+1));
											}
										}

										virtual ~CellSetCollection () {
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
		virtual bool					checkForXWings (int n, int candidate);

		// For chain coloring
		bool							checkForSinglesChainsReductions (int candidate);
		bool							checkForTwoOfTheSameColor (ChainStatusType);

		bool							validate (int level=0);

		void							print (int level);

	protected:
		virtual int						getIndex1 (int row, int col) = 0;
		virtual int						getIndex2 (int row, int col) = 0;

		CollectionType					m_type;

		std::string						m_name;
		CellSet*						m_cellSets[g_N];
};

////////////////////////////////////////////////////////////////////////////////

class AllRows : public CellSetCollection {
	public:
										AllRows () : CellSetCollection(ROW_COLLECTION, "Row") { }

		int								getIndex1 (int row, int col) { return row; }
		int								getIndex2 (int row, int col) { return col; }
};

////////////////////////////////////////////////////////////////////////////////

class AllCols : public CellSetCollection {
	public:
										AllCols () : CellSetCollection(COL_COLLECTION, "Col") { }

		int								getIndex1 (int row, int col) { return col; }
		int								getIndex2 (int row, int col) { return row; }
};

////////////////////////////////////////////////////////////////////////////////

class AllBoxes : public CellSetCollection {
	public:
										AllBoxes () : CellSetCollection(BOX_COLLECTION, "Box") { }

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

		bool							checkForXWings (int n, int candidate) { return false; }
};

////////////////////////////////////////////////////////////////////////////////

class AllCells {
	public:
										AllCells () {
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
		bool							checkForXYZWings ();

		// For chain coloring
		void							resetChains ();
		bool							buildChains (int candidate, ChainStatusType);

	protected:
		int								getIndex (int row, int col) {
											return (row * g_N) + col;
										}

		Cell*							m_cells[g_N * g_N];
};

////////////////////////////////////////////////////////////////////////////////

class SudokuSolver {
	public:
										SudokuSolver ();

		int								loadGameFile (const char* filename);
		int								loadGameString (const char* gameString);
		int								checkGameFile (const char* filename);
		int								checkGameString (const char* gameString);
		void							solve ();
		bool							tryToSolve ();
		bool							runAlgorithm (AlgorithmType algorithm);
		bool							checkForNakedSubsets (int n);
		bool							checkForHiddenSubsets (int n);
		bool							checkForLockedCandidates ();
		bool							checkForXWings (int n);
		bool							checkForYWings ();
		bool							checkForXYZWings ();

		// For singles chains
		bool							checkForSinglesChains ();
		bool							checkForSinglesChains (int candidate);
		bool							singlesChainsReduction (int candidate, ChainStatusType);
		bool							checkForSinglesChainsReductions_SameColor (int candidate, ChainStatusType);
		bool							checkForSinglesChainsReductions_DifferentColors (int candidate);
		void							printColors();

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

		CellSetCollection*				m_cellSetCollections[NUM_COLLECTIONS];
};
