// Фикстура самотеста check_no_stubs: подсказка в поле ввода (атрибут HTML) — законный код.
import React from 'react';

export function ScaleInput(props: { value: string; onChange: (v: string) => void }) {
  return (
    <input
      placeholder="1:1"
      value={props.value}
      onChange={(e) => props.onChange(e.target.value)}
    />
  );
}

export const fieldDefaults = { placeholder: 'A4', label: 'Sheet' };
