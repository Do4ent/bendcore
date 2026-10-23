/**
 * BendCore Color Scheme (11 Categories)
 * Official specification: BENDCORE_SPECIFICATION_FINAL_LOCKED.md
 * Reference Interface: Assembly-Color.dc.html (turn 312)
 */
export interface ColorCategory {
    name: string;
    hex: string;
    usage: string;
    examples: string[];
}
export declare const colorScheme: Record<string, ColorCategory>;
export declare const getColorHex: (category: string) => string;
export declare const getCategoryByUsage: (usage: string) => ColorCategory | undefined;
export declare const listAllCategories: () => ColorCategory[];
//# sourceMappingURL=ColorScheme.d.ts.map