#pragma once

#include "memory.h"

class Permutator {
	public:
		Permutator (int N) {
			m_N = N;
			m_values = (int*)malloc(m_N * sizeof(int));
			m_permutation = (int*)malloc(m_N * sizeof(int));
			m_indexes = (int*)malloc(m_N * sizeof(int));

			reset();
		}

		~Permutator () {
			free(m_values);
			free(m_permutation);
			free(m_indexes);
		}

		void reset() {
			m_numValues = 0;
			m_numInPermutation = 0;
		}

		void setValue (int value) {
			m_values[m_numValues++] = value;
		}

		int getNumValues () {
			return m_numValues;
		}

		void setNumInPermutation (int n) {
			m_numInPermutation = n;

			for (int i=0; i<n; i++) {
				m_indexes[i] = i;
			}

			m_finished = n > 0 ? false : true;
		}

		int* getNextPermutation();

	private:
		int m_N;
		bool m_finished;

		int m_numValues;
		int* m_values;

		int m_numInPermutation;
		int* m_permutation;

		int* m_indexes;
};
