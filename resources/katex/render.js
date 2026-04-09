#!/usr/bin/env node
// KaTeX LaTeX → Unicode text renderer
// Usage: echo 'E = mc^2' | node render.js
// Returns Unicode-rendered math text on stdout
const katex = require('./katex.min.js');
const fs = require('fs');

const input = fs.readFileSync(0, 'utf-8').trim();
if (!input) process.exit(0);

const SUP = {'0':'⁰','1':'¹','2':'²','3':'³','4':'⁴','5':'⁵','6':'⁶','7':'⁷','8':'⁸','9':'⁹',
    '+':'⁺','-':'⁻','=':'⁼','(':'⁽',')':'⁾','n':'ⁿ','i':'ⁱ','a':'ᵃ','b':'ᵇ','c':'ᶜ','d':'ᵈ',
    'e':'ᵉ','f':'ᶠ','g':'ᵍ','h':'ʰ','j':'ʲ','k':'ᵏ','l':'ˡ','m':'ᵐ','o':'ᵒ','p':'ᵖ',
    'r':'ʳ','s':'ˢ','t':'ᵗ','u':'ᵘ','v':'ᵛ','w':'ʷ','x':'ˣ','y':'ʸ','z':'ᶻ','T':'ᵀ'};
const SUB = {'0':'₀','1':'₁','2':'₂','3':'₃','4':'₄','5':'₅','6':'₆','7':'₇','8':'₈','9':'₉',
    '+':'₊','-':'₋','=':'₌','(':'₍',')':'₎','a':'ₐ','e':'ₑ','h':'ₕ','i':'ᵢ','j':'ⱼ',
    'k':'ₖ','l':'ₗ','m':'ₘ','n':'ₙ','o':'ₒ','p':'ₚ','r':'ᵣ','s':'ₛ','t':'ₜ','u':'ᵤ',
    'v':'ᵥ','x':'ₓ'};

function toSup(s) { return [...s].map(c => SUP[c] || c).join(''); }
function toSub(s) { return [...s].map(c => SUB[c] || c).join(''); }

try {
    const html = katex.renderToString(input, {output:'mathml', throwOnError:false});

    // Parse MathML to extract structured text
    // Simple recursive parser for <math> content
    function parseMML(s) {
        let result = '';
        // Remove the annotation (original LaTeX)
        s = s.replace(/<annotation[^>]*>[\s\S]*?<\/annotation>/g, '');
        // Remove semantics wrapper
        s = s.replace(/<\/?semantics>/g, '');
        s = s.replace(/<\/?math[^>]*>/g, '');
        s = s.replace(/<\/?mrow>/g, '');

        // Process tokens left to right
        const tokens = [];
        const re = /<(\w+)([^>]*)>([\s\S]*?)<\/\1>|([^<]+)/g;
        let m;
        while ((m = re.exec(s)) !== null) {
            if (m[4]) { // plain text
                tokens.push({type:'text', val: m[4]});
            } else {
                tokens.push({type: m[1], attrs: m[2], content: m[3]});
            }
        }

        for (const t of tokens) {
            switch(t.type) {
                case 'text':
                    result += t.val;
                    break;
                case 'mi': case 'mn': case 'mo': case 'mtext':
                    result += t.content.replace(/<[^>]+>/g, '');
                    break;
                case 'msup': {
                    const parts = splitMMLChildren(t.content);
                    result += parseMML(parts[0] || '') + toSup(parseMML(parts[1] || ''));
                    break;
                }
                case 'msub': {
                    const parts = splitMMLChildren(t.content);
                    result += parseMML(parts[0] || '') + toSub(parseMML(parts[1] || ''));
                    break;
                }
                case 'msubsup': {
                    const parts = splitMMLChildren(t.content);
                    result += parseMML(parts[0] || '') + toSub(parseMML(parts[1] || '')) + toSup(parseMML(parts[2] || ''));
                    break;
                }
                case 'mfrac': {
                    const parts = splitMMLChildren(t.content);
                    const num = parseMML(parts[0] || '');
                    const den = parseMML(parts[1] || '');
                    if (num.length > 1) result += '(' + num + ')';
                    else result += num;
                    result += '/';
                    if (den.length > 1) result += '(' + den + ')';
                    else result += den;
                    break;
                }
                case 'msqrt':
                    result += '√(' + parseMML(t.content) + ')';
                    break;
                case 'mover': {
                    const parts = splitMMLChildren(t.content);
                    result += parseMML(parts[0] || '') + '̄'; // combining overline
                    break;
                }
                case 'munder': {
                    const parts = splitMMLChildren(t.content);
                    result += parseMML(parts[0] || '') + toSub(parseMML(parts[1] || ''));
                    break;
                }
                case 'munderover': {
                    const parts = splitMMLChildren(t.content);
                    result += parseMML(parts[0] || '') + toSub(parseMML(parts[1] || '')) + toSup(parseMML(parts[2] || ''));
                    break;
                }
                case 'mtable': case 'mtr': case 'mtd':
                    result += parseMML(t.content);
                    if (t.type === 'mtd') result += '\t';
                    if (t.type === 'mtr') result += '\n';
                    break;
                case 'mspace':
                    result += ' ';
                    break;
                default:
                    result += parseMML(t.content || '');
            }
        }
        return result;
    }

    function splitMMLChildren(s) {
        // Split top-level MathML elements
        const children = [];
        let depth = 0, start = 0;
        const re2 = /<(\/?)([\w]+)[^>]*>/g;
        let m2;
        while ((m2 = re2.exec(s)) !== null) {
            if (m2[1] === '/') {
                depth--;
                if (depth === 0) {
                    children.push(s.substring(start, m2.index + m2[0].length));
                    start = m2.index + m2[0].length;
                }
            } else if (!m2[0].endsWith('/>')) {
                if (depth === 0) start = m2.index;
                depth++;
            }
        }
        return children;
    }

    // Extract the math element content
    const mathMatch = html.match(/<math[^>]*>([\s\S]*)<\/math>/);
    if (mathMatch) {
        process.stdout.write(parseMML(mathMatch[1]));
    } else {
        process.stdout.write(input); // fallback
    }
} catch(e) {
    process.stdout.write(input); // fallback to raw
}
