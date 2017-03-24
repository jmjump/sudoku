#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"

#include "Permutator.h"

int* Permutator::getNextPermutation () {
	// Have we reached the end?
	if (m_finished) {
		return NULL;
		return NULL;
	}

	// Fill in the permutation
	for (int i=0; i<m_numInPermutation; i++) {
		m_permutation[i] = m_values[m_indexes[i]];
	}

	// Advance the indexes as necessary
	m_finished = true;
	for (int i=m_numInPermutation-1; i>=0; i--) {
		int max = m_numValues - m_numInPermutation + i;
		int nextIndex = m_indexes[i] + 1;
		if (nextIndex <= max) {
			m_finished = false;

			m_indexes[i] = nextIndex;

			// next, readjust any remaining indicies
			for (i++; i<m_numInPermutation; i++) {
				m_indexes[i] = m_indexes[i-1] + 1;
			}
			break;
		}
	}

	return m_permutation;
}
