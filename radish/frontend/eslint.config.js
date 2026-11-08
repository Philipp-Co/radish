// @ts-check
// ESLint fuer das Frontend (Aufruf: npm run lint bzw. ng lint).
// Grundlage: die empfohlenen Regeln von ESLint, typescript-eslint und
// angular-eslint -- angelehnt an den Angular Style Guide und den Google
// TypeScript Style Guide (siehe README.md, Abschnitt "Code-Qualitaet").
const eslint = require("@eslint/js");
const tseslint = require("typescript-eslint");
const angular = require("angular-eslint");

module.exports = tseslint.config(
  {
    files: ["**/*.ts"],
    extends: [
      eslint.configs.recommended,
      ...tseslint.configs.recommended,
      ...tseslint.configs.stylistic,
      ...angular.configs.tsRecommended,
    ],
    processor: angular.processInlineTemplates,
    rules: {
      // Praefix "app" wie in angular.json.
      "@angular-eslint/directive-selector": ["error", { type: "attribute", prefix: "app", style: "camelCase" }],
      "@angular-eslint/component-selector": ["error", { type: "element", prefix: "app", style: "kebab-case" }],
    },
  },
  {
    files: ["**/*.html"],
    extends: [...angular.configs.templateRecommended, ...angular.configs.templateAccessibility],
  },
);
