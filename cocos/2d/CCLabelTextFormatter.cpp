/****************************************************************************
 Copyright (c) 2013      Zynga Inc.
 Copyright (c) 2013-2014 Chukong Technologies Inc.
 
 http://www.cocos2d-x.org
 
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
 ****************************************************************************/

#include "2d/CCLabel.h"
#include <vector>
#include "base/ccUTF8.h"
#include "base/CCDirector.h"
#include "2d/CCFontAtlas.h"
#include "2d/CCFontFNT.h"

NS_CC_BEGIN

void Label::computeAlignmentOffset()
{
    _linesOffsetX.clear();
    switch (_hAlignment)
    {
    case cocos2d::TextHAlignment::LEFT:
        _linesOffsetX.assign(_numberOfLines, 0);
        break;
    case cocos2d::TextHAlignment::CENTER:
        for (auto lineWidth : _linesWidth)
        {
            _linesOffsetX.push_back((_contentSize.width - lineWidth) / 2.f);
        }
        break;
    case cocos2d::TextHAlignment::RIGHT:
        for (auto lineWidth : _linesWidth)
        {
            _linesOffsetX.push_back(_contentSize.width - lineWidth);
        }
        break;
    default:
        break;
    }

    switch (_vAlignment)
    {
    case cocos2d::TextVAlignment::TOP:
        _letterOffsetY = _contentSize.height;
        break;
    case cocos2d::TextVAlignment::CENTER:
        _letterOffsetY = (_contentSize.height + _textDesiredHeight) / 2.f;
        break;
    case cocos2d::TextVAlignment::BOTTOM:
        _letterOffsetY = _textDesiredHeight;
        break;
    default:
        break;
    }
}

static int getFirstWordLen(const std::u16string& utf16Text, int startIndex, int textLen)
{
    auto character = utf16Text[startIndex];
    if (StringUtils::isCJKUnicode(character) || StringUtils::isUnicodeSpace(character) || character == '\n')
    {
        return 1;
    }

    int len = 1;
    for (int index = startIndex + 1; index < textLen; ++index)
    {
        character = utf16Text[index];
        if (character == '\n' || StringUtils::isUnicodeSpace(character) || StringUtils::isCJKUnicode(character))
        {
            break;
        }
        len++;
    }
    
    return len;
}

void Label::updateBMFontScale()
{
    auto font = _fontAtlas->getFont();
    if (_currentLabelType == LabelType::BMFONT) {
        FontFNT *bmFont = (FontFNT*)font;
        float originalFontSize = bmFont->getOriginalFontSize();
        _bmfontScale = _bmFontSize * CC_CONTENT_SCALE_FACTOR() / originalFontSize;
    }else{
        _bmfontScale = 1.0f;
    }
}

bool Label::multilineTextWrapByWord()
{
    int textLen = getStringLength();
    int lineIndex = 0;
    float nextWordX = 0.f;
    float nextWordY = 0.f;
    float longestLine = 0.f;
    float letterRight = 0.f;

    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();  
    float lineSpacing = _lineSpacing * contentScaleFactor;
    float highestY = 0.f;
    float lowestY = 0.f;
    FontLetterDefinition letterDef;
    Vec2 letterPosition;

    this->updateBMFontScale();

    for (int index = 0; index < textLen; )
    {
        auto character = _utf16Text[index];
        if (character == '\n')
        {
            _linesWidth.push_back(letterRight);
            letterRight = 0.f;
            lineIndex++;
            nextWordX = 0.f;
            nextWordY -= _lineHeight*_bmfontScale + lineSpacing;
            recordPlaceholderInfo(index, character);
            index++;
            continue;
        }

        auto wordLen = getFirstWordLen(_utf16Text, index, textLen);
        float wordHighestY = highestY;;
        float wordLowestY = lowestY;
        float wordRight = letterRight;
        float nextLetterX = nextWordX;
        bool newLine = false;
        for (int tmp = 0; tmp < wordLen;++tmp)
        {
            int letterIndex = index + tmp;
            character = _utf16Text[letterIndex];
            if (character == '\r')
            {
                recordPlaceholderInfo(letterIndex, character);
                continue;
            }
            if (_fontAtlas->getLetterDefinitionForChar(character, letterDef) == false)
            {
                recordPlaceholderInfo(letterIndex, character);
                CCLOG("LabelTextFormatter error:can't find letter definition in font file for letter: %c", character);
                continue;
            }

            auto letterX = (nextLetterX + letterDef.offsetX * _bmfontScale) / contentScaleFactor;
            if (_enableWrap && _maxLineWidth > 0.f && nextWordX > 0.f && letterX + letterDef.width * _bmfontScale > _maxLineWidth)
            {
                _linesWidth.push_back(letterRight);
                letterRight = 0.f;
                lineIndex++;
                nextWordX = 0.f;
                nextWordY -= (_lineHeight*_bmfontScale + lineSpacing);
                newLine = true;
                break;
            }
            else
            {
                letterPosition.x = letterX;
            }
            letterPosition.y = (nextWordY - letterDef.offsetY * _bmfontScale) / contentScaleFactor;
            recordLetterInfo(letterPosition, character, letterIndex, lineIndex);

            if (_horizontalKernings && letterIndex < textLen - 1)
                nextLetterX += _horizontalKernings[letterIndex + 1];
            nextLetterX += letterDef.xAdvance * _bmfontScale + _additionalKerning;

            wordRight = letterPosition.x + letterDef.width * _bmfontScale;

            if (wordHighestY < letterPosition.y)
                wordHighestY = letterPosition.y;
            if (wordLowestY > letterPosition.y - letterDef.height * _bmfontScale)
                wordLowestY = letterPosition.y - letterDef.height * _bmfontScale;
        }
        
        if (newLine)
        {
            continue;
        }

        nextWordX = nextLetterX;
        letterRight = wordRight;
        if (highestY < wordHighestY)
            highestY = wordHighestY;
        if (lowestY > wordLowestY)
            lowestY = wordLowestY;
        if (longestLine < letterRight)
            longestLine = letterRight;

        index += wordLen;
    }

    _linesWidth.push_back(letterRight);

    _numberOfLines = lineIndex + 1;
    _textDesiredHeight = (_numberOfLines * _lineHeight * _bmfontScale) / contentScaleFactor;
    if (_numberOfLines > 1)
        _textDesiredHeight += (_numberOfLines - 1) * _lineSpacing;
    Size contentSize(_labelWidth, _labelHeight);
    if (_labelWidth <= 0.f)
        contentSize.width = longestLine;
    if (_labelHeight <= 0.f)
        contentSize.height = _textDesiredHeight;
    
    setContentSize(contentSize);

    _tailoredTopY = contentSize.height;
    _tailoredBottomY = 0.f;
    if (highestY > 0.f)
        _tailoredTopY = contentSize.height + highestY;
    if (lowestY < -_textDesiredHeight)
        _tailoredBottomY = _textDesiredHeight + lowestY;

    return true;
}

bool Label::multilineTextWrapByChar()
{
    int textLen = getStringLength();
    int lineIndex = 0;
    float nextLetterX = 0.f;
    float nextLetterY = 0.f;
    float longestLine = 0.f;
    float letterRight = 0.f;

    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();
    float lineSpacing = _lineSpacing * contentScaleFactor;
    float highestY = 0.f;
    float lowestY = 0.f;
    FontLetterDefinition letterDef;
    Vec2 letterPosition;

    this->updateBMFontScale();

    for (int index = 0; index < textLen; index++)
    {
        auto character = _utf16Text[index];
        if (character == '\r')
        {
            recordPlaceholderInfo(index, character);
            continue;
        }
        if (character == '\n')
        {
            _linesWidth.push_back(letterRight);
            letterRight = 0.f;
            lineIndex++;
            nextLetterX = 0.f;
            nextLetterY -= _lineHeight * _bmfontScale + lineSpacing;
            recordPlaceholderInfo(index, character);
            continue;
        }

        if (_fontAtlas->getLetterDefinitionForChar(character, letterDef) == false)
        {
            recordPlaceholderInfo(index, character);
            CCLOG("LabelTextFormatter error:can't find letter definition in font file for letter: %c", character);
            continue;
        }

        auto letterX = (nextLetterX + letterDef.offsetX * _bmfontScale) / contentScaleFactor;
        if (_enableWrap && _maxLineWidth > 0.f && nextLetterX > 0.f && letterX + letterDef.width * _bmfontScale > _maxLineWidth)
        {
            _linesWidth.push_back(letterRight);
            letterRight = 0.f;
            lineIndex++;
            nextLetterX = 0.f;
            nextLetterY -= _lineHeight * _bmfontScale + lineSpacing;
            letterPosition.x = letterDef.offsetX * _bmfontScale / contentScaleFactor;
        }
        else
        {
            letterPosition.x = letterX;
        }
        letterPosition.y = (nextLetterY - letterDef.offsetY * _bmfontScale) / contentScaleFactor;
        recordLetterInfo(letterPosition, character, index, lineIndex);

        if (_horizontalKernings && index < textLen - 1)
            nextLetterX += _horizontalKernings[index + 1];
        nextLetterX += letterDef.xAdvance * _bmfontScale + _additionalKerning;

        letterRight = letterPosition.x + letterDef.width * _bmfontScale;

        if (highestY < letterPosition.y)
            highestY = letterPosition.y;
        if (lowestY > letterPosition.y - letterDef.height * _bmfontScale)
            lowestY = letterPosition.y - letterDef.height * _bmfontScale;
        if (longestLine < letterRight)
            longestLine = letterRight;
    }

    _linesWidth.push_back(letterRight);

    _numberOfLines = lineIndex + 1;
    _textDesiredHeight = (_numberOfLines * _lineHeight * _bmfontScale) / contentScaleFactor;
    if (_numberOfLines > 1)
        _textDesiredHeight += (_numberOfLines - 1) * _lineSpacing;
    Size contentSize(_labelWidth, _labelHeight);
    if (_labelWidth <= 0.f)
        contentSize.width = longestLine;
    if (_labelHeight <= 0.f)
        contentSize.height = _textDesiredHeight;

    setContentSize(contentSize);

    _tailoredTopY = contentSize.height;
    _tailoredBottomY = 0.f;
    if (highestY > 0.f)
        _tailoredTopY = contentSize.height + highestY;
    if (lowestY < -_textDesiredHeight)
        _tailoredBottomY = _textDesiredHeight + lowestY;

    return true;
}

bool Label::isVerticalClampByWord()
{
    int textLen = getStringLength();
    int lineIndex = 0;
    float nextWordX = 0.f;
    float letterRight = 0.f;
    
    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();
    FontLetterDefinition letterDef;
    Vec2 letterPosition;
    
    this->updateBMFontScale();
    
    for (int index = 0; index < textLen; )
    {
        auto character = _utf16Text[index];
        if (character == '\n')
        {
            letterRight = 0.f;
            lineIndex++;
            nextWordX = 0.f;
            index++;
            continue;
        }
        
        auto wordLen = getFirstWordLen(_utf16Text, index, textLen);
        float wordRight = letterRight;
        float nextLetterX = nextWordX;
        bool newLine = false;
        for (int tmp = 0; tmp < wordLen;++tmp)
        {
            int letterIndex = index + tmp;
            character = _utf16Text[letterIndex];
            if (character == '\r')
            {
                continue;
            }
            if (_fontAtlas->getLetterDefinitionForChar(character, letterDef) == false)
            {
                CCLOG("LabelTextFormatter error:can't find letter definition in font file for letter: %c", character);
                continue;
            }
            
            auto letterX = (nextLetterX + letterDef.offsetX * _bmfontScale) / contentScaleFactor;
            if (_maxLineWidth > 0.f
                && nextWordX > 0.f
                && letterX + letterDef.width * _bmfontScale > _maxLineWidth)
            {
                letterRight = 0.f;
                lineIndex++;
                nextWordX = 0.f;
                newLine = true;
                break;
            }
            else
            {
                letterPosition.x = letterX;
            }
            if (_horizontalKernings && letterIndex < textLen - 1)
                nextLetterX += _horizontalKernings[letterIndex + 1];
            nextLetterX += letterDef.xAdvance * _bmfontScale + _additionalKerning;
            
            wordRight = letterPosition.x + letterDef.width * _bmfontScale;
            
        }
        
        if (newLine)
        {
            continue;
        }
        
        nextWordX = nextLetterX;
        letterRight = wordRight;
        
        index += wordLen;
    }
    
    _numberOfLines = lineIndex + 1;
    _textDesiredHeight = (_numberOfLines * _lineHeight * _bmfontScale) / contentScaleFactor;
    if (_numberOfLines > 1)
        _textDesiredHeight += (_numberOfLines - 1) * _lineSpacing;
    
    if (_textDesiredHeight > _labelHeight)
    {
        return false;
    }
    else
    {
        return true;
    }

}

bool Label::isVerticalClampByChar()
{
    int textLen = getStringLength();
    int lineIndex = 0;
    float nextLetterX = 0.f;
    float letterRight = 0.f;
    
    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();
    FontLetterDefinition letterDef;
    Vec2 letterPosition;
    
    this->updateBMFontScale();

    for (int index = 0; index < textLen; index++)
    {
        auto character = _utf16Text[index];
        if (character == '\r')
        {
            continue;
        }
        if (character == '\n')
        {
            letterRight = 0.f;
            lineIndex++;
            nextLetterX = 0.f;
            continue;
        }
        
        if (_fontAtlas->getLetterDefinitionForChar(character, letterDef) == false)
        {
            recordPlaceholderInfo(index, character);
            CCLOG("LabelTextFormatter error:can't find letter definition in font file for letter: %c", character);
            continue;
        }
        
        auto letterX = (nextLetterX + letterDef.offsetX * _bmfontScale) / contentScaleFactor;
        if (_maxLineWidth > 0.f
            && nextLetterX > 0.f
            && letterX + letterDef.width * _bmfontScale > _maxLineWidth)
        {
            letterRight = 0.f;
            lineIndex++;
            nextLetterX = 0.f;
            letterPosition.x = letterDef.offsetX * _bmfontScale / contentScaleFactor;
        }
        else
        {
            letterPosition.x = letterX;
        }
        
        if (_horizontalKernings && index < textLen - 1)
            nextLetterX += _horizontalKernings[index + 1];
        nextLetterX += letterDef.xAdvance * _bmfontScale + _additionalKerning;
        
        letterRight = letterPosition.x + letterDef.width * _bmfontScale;
    }
    
    _numberOfLines = lineIndex + 1;
    _textDesiredHeight = (_numberOfLines * _lineHeight * _bmfontScale) / contentScaleFactor;
    if (_numberOfLines > 1)
        _textDesiredHeight += (_numberOfLines - 1) * _lineSpacing;
    
    if (_textDesiredHeight > _labelHeight)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool Label::isHorizontalClamp()
{
    bool letterClamp = false;
    for (int ctr = 0; ctr < _lengthOfString; ++ctr)
    {
        if (_lettersInfo[ctr].valid)
        {
            auto& letterDef = _fontAtlas->_letterDefinitions[_lettersInfo[ctr].utf16Char];
            
            auto px = _lettersInfo[ctr].positionX + letterDef.width/2;
            if(_labelWidth > 0.f){
                if (px > _contentSize.width) {
                    letterClamp = true;
                    break;
                }
            }
        }
    }
    
    return letterClamp;
}

void Label::shrinkLabelToContentSize()
{
    float fontSize = 0;
    if (_currentLabelType == LabelType::BMFONT) {
        fontSize = _bmFontSize;
    }else if(_currentLabelType == LabelType::TTF){
        fontSize = this->getTTFConfig().fontSize;
    }
    
    int i = 0;
    auto letterDefinition = _fontAtlas->_letterDefinitions;
    auto tempLetterDefinition = letterDefinition;
    bool flag = true;
    while (isHorizontalClamp()) {
        ++i;
        float newFontSize = fontSize - i;
        flag = false;
        float scale = newFontSize / fontSize;
        std::swap(_fontAtlas->_letterDefinitions, tempLetterDefinition);
        _fontAtlas->scaleFontLetterDefinition(scale);
        
        if (_maxLineWidth > 0.f && !_lineBreakWithoutSpaces)
        {
            multilineTextWrapByWord();
        }
        else
        {
            multilineTextWrapByChar();
        }
        computeAlignmentOffset();
        tempLetterDefinition = letterDefinition;
    }
    std::swap(_fontAtlas->_letterDefinitions, letterDefinition);

    if (!flag) {
        this->scaleFontSizeDown(fontSize - i);
    }
}

void Label::recordLetterInfo(const cocos2d::Vec2& point, char16_t utf16Char, int letterIndex, int lineIndex)
{
    if (static_cast<std::size_t>(letterIndex) >= _lettersInfo.size())
    {
        LetterInfo tmpInfo;
        _lettersInfo.push_back(tmpInfo);
    }
    _lettersInfo[letterIndex].lineIndex = lineIndex;
    _lettersInfo[letterIndex].utf16Char = utf16Char;
    _lettersInfo[letterIndex].valid = _fontAtlas->_letterDefinitions[utf16Char].validDefinition;
    _lettersInfo[letterIndex].positionX = point.x;
    _lettersInfo[letterIndex].positionY = point.y;
}

void Label::recordPlaceholderInfo(int letterIndex, char16_t utf16Char)
{
    if (static_cast<std::size_t>(letterIndex) >= _lettersInfo.size())
    {
        LetterInfo tmpInfo;
        _lettersInfo.push_back(tmpInfo);
    }
    _lettersInfo[letterIndex].utf16Char = utf16Char;
    _lettersInfo[letterIndex].valid = false;
}

NS_CC_END
