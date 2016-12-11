# Implementation-Sed
V jazyku C/C++ implementujte vlastnú verziu príkazu sed. Implementácia musí využiť vlákna na urýchlenie činnosti aplikácie,
ak táto je spustená na viacjadrovom systéme. Vstupom je vzorka textu, ktorá sa má nahradiť vo všetkých riadkoch všetkých zadaných
súborov a reťazec, ktorým ju treba nahradiť. Štandardne program kopíruje všetky riadky všetkých zadaných súborov na štandardný výstup,
pričom výstup je generovaný podľa zadaných prepínačov. Program by mal rozpoznávať minimálne nasledujúce prepínače:

 * s – vykoná náhradu na mieste, t.j. nevypisuje výsledok na štandardný výstup, ale výsledkom bude nahradenie vzorky priamo v zadanom súbore.
 * i – ignoruje veľké/malé písmená pri porovnávaní riadkov so vzorkou
