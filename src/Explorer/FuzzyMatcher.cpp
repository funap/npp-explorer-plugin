/*
  The MIT License (MIT)
  
  Copyright (c) 2019 funap
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "FuzzyMatcher.h"

#include <cwctype>
#include <memory>
#include <algorithm>

FuzzyMatcher::FuzzyMatcher(std::wstring_view pattern)
    : pattern_(pattern)
{
}

FuzzyMatcher::~FuzzyMatcher()
{
}

int FuzzyMatcher::ScoreMatch(std::wstring_view target, std::vector<size_t>* positions)
{
    if (0 == pattern_.length()) {
        return 0;
    }
    if (0 == target.length()) {
        return 0;
    }
    if (pattern_.length() > target.length()) {
        return 0;
    }

    auto scoreMatrix = std::make_unique<int[]>(pattern_.length() * target.length());
    auto matcheMatrix = std::make_unique<int[]>(pattern_.length() * target.length());

    for (size_t patternIndex = 0; patternIndex < pattern_.length(); ++patternIndex) {
        const bool patternIsFirstIndex          = (0 == patternIndex);
        const size_t patternIndexOffset         = patternIndex * target.length();
        const size_t patternIndexPreviousOffset = patternIndexOffset - target.length();

        for (size_t targetIndex = 0; targetIndex < target.length(); ++targetIndex) {
            const bool targetIsFirstIndex       = (0 == targetIndex);
            const size_t currentIndex           = patternIndexOffset + targetIndex;
            const size_t leftIndex              = currentIndex - 1;
            const size_t diagIndex              = patternIndexPreviousOffset + (targetIndex - 1);

            const int leftScore                 = targetIsFirstIndex ? 0 : scoreMatrix[leftIndex];
            const int diagScore                 = (patternIsFirstIndex || targetIsFirstIndex) ? 0 : scoreMatrix[diagIndex];
            const int matchesSequenceLength     = (patternIsFirstIndex || targetIsFirstIndex) ? 0 : matcheMatrix[diagIndex];

            int score;
            if (!diagScore && !patternIsFirstIndex) {
                score = 0;
            }
            else {
                score = CalculateScore(pattern_[patternIndex], target, targetIndex, matchesSequenceLength);
            }

            if (score && (leftScore <= diagScore + score)) {
                matcheMatrix[currentIndex] = matchesSequenceLength + 1;
                scoreMatrix[currentIndex] = diagScore + score;
            }
            else {
                matcheMatrix[currentIndex] = 0;
                scoreMatrix[currentIndex] = leftScore;
            }
        }
    }
    const int result = scoreMatrix[pattern_.length() * target.length() - 1];

    // Restore Positions
    if (positions) {
        size_t patternIndex = pattern_.length() - 1;
        size_t targetIndex = target.length() - 1;
        while ((0 <= patternIndex) && (0 <= targetIndex)) {
            const size_t currentIndex = patternIndex * target.length() + targetIndex;
            const int match = matcheMatrix[currentIndex];
            if (0 == match) {
                if (0 < targetIndex) {
                    --targetIndex;    // go left
                }
                else {
                    break;
                }
            }
            else {
                positions->emplace_back(targetIndex);

                if ((0 < patternIndex) && (0 < targetIndex)) {
                    --patternIndex;
                    --targetIndex;    // go up and left
                }
                else {
                    break;
                }
            }
        }
        std::reverse(positions->begin(), positions->end());
    }

    return result;
}


int FuzzyMatcher::CalculateScore(wchar_t patternChar, const std::wstring_view &target, size_t targetIndex, int matchesSequenceLength)
{
    int score = 0;

    constexpr int CHARACTER_MATCH_BONUS     = 1;
    constexpr int SAME_CASE_BONUS           = 3;
    constexpr int FIRST_LETTER_BONUS        = 13;
    constexpr int CONSECUTIVE_MATCH_BONUS   = 5;
    constexpr int START_OF_EXTENSION_BONUS  = 3;
    constexpr int CAMEL_CASE_BONUS          = 10;
    constexpr int SEPARATOR_BONUS           = 10;

    const wchar_t patternLowerChar = std::towlower(patternChar);
    const wchar_t targetLowerChar = std::towlower(target[targetIndex]);
    if (patternLowerChar != targetLowerChar) {
        return score; // no match
    }
    score += CHARACTER_MATCH_BONUS;

    if (0 < matchesSequenceLength) {
        score += (matchesSequenceLength * CONSECUTIVE_MATCH_BONUS);
    }

    if (patternChar == target[targetIndex]) {
        score += SAME_CASE_BONUS;
    }

    if (0 == targetIndex) {
        score += FIRST_LETTER_BONUS;
    }
    else {
        switch (target[targetIndex - 1]) {
        case ' ':
        case '_':
            score += SEPARATOR_BONUS;
            break;
        case '.':
            score += START_OF_EXTENSION_BONUS;
            break;
        default:
            if (std::iswlower(target[targetIndex - 1])) {
                if (std::iswupper(target[targetIndex])) {
                    score += CAMEL_CASE_BONUS;
                }
            }
            break;
        }
    }

    return score;
};