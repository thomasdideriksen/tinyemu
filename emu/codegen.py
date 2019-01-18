import sys
import json

def makeModeBitPattern(mode, reg, swapped):
    return ((reg << 3) | mode) if swapped else ((mode << 3) | reg)

def modesToValid(modes, swapped):
    result = []
    for mode in modes:
        if mode <= 6:
            for reg in range(0, 8):
                bitPattern = makeModeBitPattern(mode, reg, swapped)
                result.append(bitPattern)
        else:
            bitPattern = makeModeBitPattern(7, mode - 7, swapped)
            result.append(bitPattern)
    return result

def makeBitPatterns(opcode, pieceIndex, bitIndex, combinedValue):
    result = []
    pattern = opcode['pattern']
    if pieceIndex == len(pattern):
        if bitIndex != 16:
            raise Exception('Incorrect bit count in {}'.format(opcode['name']))
        result.append(combinedValue)
    else:
        piece = pattern[pieceIndex]
        if 'modes' in piece:
             piece['valid'] = modesToValid(piece['modes'], piece.get('swapped', False))
        if not 'valid' in piece:
            piece['valid'] = []
        if len(piece['valid']) == 0:
            maxValue = pow(2, piece['bits'])
            for i in range(0, maxValue):
                piece['valid'].append(i)
        for value in piece['valid']:
            bitPatterns = makeBitPatterns(
                opcode, 
                pieceIndex + 1, 
                bitIndex + piece['bits'], 
                combinedValue | (value << (16 - piece['bits'] - bitIndex)))
            result.extend(bitPatterns)
    return result
    
def getTemplateParams(opcode, bitPattern):
    result = []
    pattern = opcode['pattern']
    bitIndex = 0
    for piece in pattern:
        bits = piece['bits']
        bitIndex += bits
        if piece.get('excludeTemplate', False):
            continue
        if len(piece['valid']) > 1 or piece.get('forceTemplate', False) == True:
            param = (bitPattern >> (16 - bitIndex)) & (0xffff >> (16 - bits))
            if 'mapping' in piece:
                param = piece['mapping'][str(param)]
            result.append(param)
    return result

try:

    with open('opcodes.json', 'r') as f:
        opcodes = json.load(f)

    with open('generated.cpp', 'w') as f:
        occupied = {}
        for opcode in opcodes:
            bitPatterns = makeBitPatterns(opcode, 0, 0, 0)
            for bitPattern in bitPatterns:
                if bitPattern in occupied:
                    conflict = occupied[bitPattern]
                    raise Exception('Bit pattern ({:016b}) for [{}] is already in use by [{}]'.format(bitPattern, opcode['name'], conflict['name']))
                occupied[bitPattern] = opcode
                templateParams = getTemplateParams(opcode, bitPattern)
                if len(templateParams) == 0:
                    code = 'table[{:#06x}] = {};\n'.format(bitPattern, opcode['name'])
                else:
                    code = 'table[{:#06x}] = {}<{}' + ', {}' * (len(templateParams) - 1) + '>;\n'
                    code = code.format(bitPattern, opcode['name'], *templateParams)
                f.write(code);

except Exception as ex:
    print('error: {}'.format(ex))
    sys.exit(1)
