// The MIT License (MIT)
//
// Copyright (c) 2019-2024 funap
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "FuzzyMatcher.h"

#include <cwctype>
#include <memory>
#include <algorithm>

namespace {
    struct ScoringConstants {
        static constexpr int CHARACTER_MATCH_BONUS      = 1;
        static constexpr int SAME_CASE_BONUS            = 1;
        static constexpr int FIRST_LETTER_BONUS         = 8;
        static constexpr int CONSECUTIVE_MATCH_BONUS    = 5;
        static constexpr int START_OF_EXTENSION_BONUS   = 3;
        static constexpr int CAMEL_CASE_BONUS           = 4;
        static constexpr int SEPARATOR_BONUS            = 4;
        static constexpr int DIRECTORY_SEPARATOR_BONUS  = 5;
    };

    bool ValidateInputs(const std::wstring_view& pattern, const std::wstring_view& target)
    {
        return !(pattern.empty() || target.empty() || pattern.length() > target.length());
    }

    void RestoreMatchPositions(std::vector<size_t>* positions,
                             const int* matchMatrix,
                             size_t patternLength,
                             size_t targetLength)
    {
        if (!positions) return;

        size_t patternIndex = patternLength - 1;
        size_t targetIndex  = targetLength  - 1;

        while ((0 <= patternIndex) && (0 <= targetIndex)) {
            const size_t currentIndex = patternIndex * targetLength + targetIndex;
            const int match = matchMatrix[currentIndex];

            if (0 == match) {
                if (0 < targetIndex) {
                    --targetIndex;
                }
                else {
                    break;
                }
            }
            else {
                positions->emplace_back(targetIndex);
                if ((0 < patternIndex) && (0 < targetIndex)) {
                    --patternIndex;
                    --targetIndex;
                }
                else {
                    break;
                }
            }
        }
        std::reverse(positions->begin(), positions->end());
    }
} // namespace

FuzzyMatcher::FuzzyMatcher(std::wstring_view pattern)
    : pattern_(pattern)
    , scoreMatrix_()
    , matchMatrix_()
{
}

FuzzyMatcher::~FuzzyMatcher() = default;

int FuzzyMatcher::ScoreMatch(std::wstring_view target, std::vector<size_t>* positions)
{
    if (!ValidateInputs(pattern_, target)) {
        return 0;
    }

    scoreMatrix_.resize(pattern_.length() * target.length());
    matchMatrix_.resize(pattern_.length() * target.length());
    for (size_t patternIndex = 0; patternIndex < pattern_.length(); ++patternIndex) {
        const bool patternIsFirstIndex          = (0 == patternIndex);
        const size_t patternIndexOffset         = patternIndex * target.length();
        const size_t patternIndexPreviousOffset = patternIndexOffset - target.length();

        for (size_t targetIndex = 0; targetIndex < target.length(); ++targetIndex) {
            const bool targetIsFirstIndex   = (0 == targetIndex);
            const size_t currentIndex       = patternIndexOffset + targetIndex;
            const size_t leftIndex          = currentIndex - 1;
            const size_t diagIndex          = patternIndexPreviousOffset + (targetIndex - 1);

            const int leftScore = targetIsFirstIndex ? 0 : scoreMatrix_[leftIndex];
            const int diagScore = (patternIsFirstIndex || targetIsFirstIndex) ? 0 : scoreMatrix_[diagIndex];
            const int matchesSequenceLength = (patternIsFirstIndex || targetIsFirstIndex) ? 0 : matchMatrix_[diagIndex];

            const int score = (!diagScore && !patternIsFirstIndex)
                            ? 0
                            : CalculateScore(pattern_[patternIndex], target, targetIndex, matchesSequenceLength);

            if (score && (leftScore <= diagScore + score)) {
                matchMatrix_[currentIndex] = matchesSequenceLength + 1;
                scoreMatrix_[currentIndex] = diagScore + score;
            }
            else {
                matchMatrix_[currentIndex] = 0;
                scoreMatrix_[currentIndex] = leftScore;
            }
        }
    }

    const int result = scoreMatrix_[pattern_.length() * target.length() - 1];
    RestoreMatchPositions(positions, matchMatrix_.data(), pattern_.length(), target.length());
    return result;
}

int FuzzyMatcher::CalculateScore(wchar_t patternChar, const std::wstring_view& target, size_t targetIndex, int matchesSequenceLength)
{
    int score = 0;

    const wchar_t patternLowerChar = std::towlower(patternChar);
    const wchar_t targetLowerChar = std::towlower(target[targetIndex]);

    if (patternLowerChar != targetLowerChar) {
        return score;
    }

    score += ScoringConstants::CHARACTER_MATCH_BONUS;

    if (0 < matchesSequenceLength) {
        score += (matchesSequenceLength * ScoringConstants::CONSECUTIVE_MATCH_BONUS);
    }

    if (patternChar == target[targetIndex]) {
        score += ScoringConstants::SAME_CASE_BONUS;
    }

    if (0 == targetIndex) {
        score += ScoringConstants::FIRST_LETTER_BONUS;
    }
    else {
        switch (target[targetIndex - 1]) {
        case '\\':
            score += ScoringConstants::DIRECTORY_SEPARATOR_BONUS;
            break;
        case ' ':
        case '_':
            score += ScoringConstants::SEPARATOR_BONUS;
            break;
        case '.':
            score += ScoringConstants::START_OF_EXTENSION_BONUS;
            break;
        default:
            if (std::iswlower(target[targetIndex - 1]) && std::iswupper(target[targetIndex])) {
                score += ScoringConstants::CAMEL_CASE_BONUS;
            }
            break;
        }
    }

    return score;
}
