const fs = require('fs');
const path = require('path');

const xmlPath = path.join(__dirname, 'temp_tbl', 'word', 'document.xml');
if (!fs.existsSync(xmlPath)) {
    console.error('File not found:', xmlPath);
    process.exit(1);
}

const xml = fs.readFileSync(xmlPath, 'utf8');

// A simple parser to find tables (<w:tbl> ... </w:tbl>)
const tblRegex = /<w:tbl[^>]*>([\s\S]*?)<\/w:tbl>/g;
let match;
let tblCount = 0;

while ((match = tblRegex.exec(xml)) !== null) {
    tblCount++;
    console.log(`\n=== Table ${tblCount} ===`);
    const tblXml = match[1];
    
    // Find all rows (<w:tr> ... </w:tr>)
    const trRegex = /<w:tr[^>]*>([\s\S]*?)<\/w:tr>/g;
    let trMatch;
    
    while ((trMatch = trRegex.exec(tblXml)) !== null) {
        const trXml = trMatch[1];
        
        // Find all cells (<w:tc> ... </w:tc>)
        const tcRegex = /<w:tc[^>]*>([\s\S]*?)<\/w:tc>/g;
        let tcMatch;
        let rowCells = [];
        
        while ((tcMatch = tcRegex.exec(trXml)) !== null) {
            const tcXml = tcMatch[1];
            // Extract all text in this cell
            const tMatches = tcXml.match(/<w:t[^>]*>(.*?)<\/w:t>/g);
            const cellText = tMatches ? tMatches.map(m => m.replace(/<w:t[^>]*>/, '').replace(/<\/w:t>/, '')).join(' ') : '';
            rowCells.push(cellText.trim());
        }
        console.log('| ' + rowCells.join(' | ') + ' |');
    }
}
