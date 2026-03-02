import { chromium } from '/opt/node22/lib/node_modules/playwright/index.mjs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const htmlPath = resolve(__dirname, 'manual_tecnico.html');
const pdfPath  = resolve(__dirname, 'manual_tecnico.pdf');

const browser = await chromium.launch({ headless: true });
const page    = await browser.newPage();

await page.goto(`file://${htmlPath}`, { waitUntil: 'networkidle' });

// Allow CSS animations to finish
await page.waitForTimeout(500);

await page.pdf({
  path:   pdfPath,
  format: 'A4',
  printBackground: true,
  margin: { top: '22mm', right: '18mm', bottom: '20mm', left: '22mm' },
  displayHeaderFooter: true,
  headerTemplate: '<span></span>',
  footerTemplate: `
    <div style="width:100%; font-size:8pt; color:#9AA0A6;
                display:flex; justify-content:space-between; padding:0 18mm;">
      <span>Otto Ninja Pro Controller – Manual Técnico v2.0.0</span>
      <span><span class="pageNumber"></span> / <span class="totalPages"></span></span>
    </div>`,
});

await browser.close();
console.log(`PDF generado: ${pdfPath}`);
