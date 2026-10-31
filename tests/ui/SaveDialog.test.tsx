import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import SaveDialog from '../../src/ui/SaveDialog';

/**
 * SaveDialog Test Suite - GAP 080
 * Comprehensive tests for save dialog with format selection
 * 32 test cases
 */

describe('SaveDialog Component', () => {
  const mockOnClose = jest.fn();
  const mockOnSave = jest.fn();

  beforeEach(() => {
    jest.clearAllMocks();
  });

  describe('Rendering', () => {
    test('should not render when isOpen is false', () => {
      const { container } = render(
        <SaveDialog isOpen={false} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(container.querySelector('.save-dialog')).not.toBeInTheDocument();
    });

    test('should render when isOpen is true', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(screen.getByText('Save File')).toBeInTheDocument();
    });

    test('should render all required input fields', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(screen.getByLabelText('Save location:')).toBeInTheDocument();
      expect(screen.getByLabelText('File name:')).toBeInTheDocument();
      expect(screen.getByLabelText('File format:')).toBeInTheDocument();
    });

    test('should render save and cancel buttons', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(screen.getByRole('button', { name: 'Save' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'Cancel' })).toBeInTheDocument();
    });
  });

  describe('File Name Input', () => {
    test('should set default file name', () => {
      render(
        <SaveDialog
          isOpen={true}
          onClose={mockOnClose}
          onSave={mockOnSave}
          defaultFileName="my_project"
        />
      );
      const input = screen.getByDisplayValue('my_project');
      expect(input).toBeInTheDocument();
    });

    test('should update file name on user input', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'newfile' } });
      expect(input.value).toBe('newfile');
    });

    test('should reject invalid file names (with slashes)', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'invalid/name' } });
      expect(screen.getByText('Please enter a valid file name')).toBeInTheDocument();
    });

    test('should reject empty file names', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: '' } });
      const saveBtn = screen.getByRole('button', { name: 'Save' });
      expect(saveBtn).toBeDisabled();
    });

    test('should display file extension automatically', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(screen.getByText('.bnd')).toBeInTheDocument();
    });
  });

  describe('File Format Selection', () => {
    test('should display default format options', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(screen.getByText(/BendCore Native/)).toBeInTheDocument();
      expect(screen.getByText(/STEP/)).toBeInTheDocument();
      expect(screen.getByText(/IGES/)).toBeInTheDocument();
      expect(screen.getByText(/STL/)).toBeInTheDocument();
    });

    test('should accept custom format options', () => {
      const customFormats = [
        { format: 'Custom Format', extension: '.custom', description: 'Custom file format' }
      ];
      render(
        <SaveDialog
          isOpen={true}
          onClose={mockOnClose}
          onSave={mockOnSave}
          supportedFormats={customFormats}
        />
      );
      expect(screen.getByText(/Custom Format/)).toBeInTheDocument();
    });

    test('should change extension when format is selected', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const formatSelect = screen.getByLabelText('File format:') as HTMLSelectElement;
      fireEvent.change(formatSelect, { target: { value: '.step' } });
      expect(screen.getByText('.step')).toBeInTheDocument();
    });

    test('should set first format as default', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const formatSelect = screen.getByLabelText('File format:') as HTMLSelectElement;
      expect(formatSelect.value).toBe('.bnd');
    });
  });

  describe('Save Path', () => {
    test('should set default save path', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const pathInput = screen.getByDisplayValue('/home') as HTMLInputElement;
      expect(pathInput).toBeInTheDocument();
    });

    test('should update save path on user input', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const pathInput = screen.getByPlaceholderText('Enter save path...') as HTMLInputElement;
      fireEvent.change(pathInput, { target: { value: '/documents/projects' } });
      expect(pathInput.value).toBe('/documents/projects');
    });

    test('should display browse button', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const browseBtn = screen.getByTitle('Browse...');
      expect(browseBtn).toBeInTheDocument();
    });
  });

  describe('File Preview', () => {
    test('should display file preview section', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(screen.getByText('Preview:')).toBeInTheDocument();
    });

    test('should show full path in preview', () => {
      render(
        <SaveDialog
          isOpen={true}
          onClose={mockOnClose}
          onSave={mockOnSave}
          defaultFileName="myfile"
        />
      );
      expect(screen.getByText('/home/myfile.bnd')).toBeInTheDocument();
    });

    test('should update preview when file name changes', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'newname' } });
      expect(screen.getByText('/home/newname.bnd')).toBeInTheDocument();
    });

    test('should update preview when format changes', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const formatSelect = screen.getByLabelText('File format:') as HTMLSelectElement;
      fireEvent.change(formatSelect, { target: { value: '.step' } });
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'myfile' } });
      expect(screen.getByText('/home/myfile.step')).toBeInTheDocument();
    });

    test('should display format description in preview', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      expect(screen.getByTestId('format-description')).toHaveTextContent(/BendCore project file/);
    });
  });

  describe('Save As Option', () => {
    test('should display save with new name checkbox', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const checkbox = screen.getByRole('checkbox');
      expect(checkbox).toBeInTheDocument();
    });

    test('should toggle save as name option', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const checkbox = screen.getByRole('checkbox') as HTMLInputElement;
      fireEvent.click(checkbox);
      expect(checkbox.checked).toBe(true);
    });
  });

  describe('Dialog Actions', () => {
    test('should call onClose when Cancel button is clicked', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const cancelBtn = screen.getByRole('button', { name: 'Cancel' });
      fireEvent.click(cancelBtn);
      expect(mockOnClose).toHaveBeenCalled();
    });

    test('should call onClose when close button is clicked', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const closeBtn = screen.getByText('×');
      fireEvent.click(closeBtn);
      expect(mockOnClose).toHaveBeenCalled();
    });

    test('should disable Save button when file name is invalid', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: '' } });
      const saveBtn = screen.getByRole('button', { name: 'Save' });
      expect(saveBtn).toBeDisabled();
    });

    test('should enable Save button when file name is valid', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'validname' } });
      const saveBtn = screen.getByRole('button', { name: 'Save' });
      expect(saveBtn).not.toBeDisabled();
    });
  });

  describe('Save Functionality', () => {
    test('should call onSave with correct parameters', async () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'myfile' } });

      const saveBtn = screen.getByRole('button', { name: 'Save' });
      fireEvent.click(saveBtn);

      await waitFor(() => {
        expect(mockOnSave).toHaveBeenCalledWith('/home/myfile.bnd', '.bnd');
      });
    });

    test('should call onSave with selected format', async () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const formatSelect = screen.getByLabelText('File format:') as HTMLSelectElement;
      fireEvent.change(formatSelect, { target: { value: '.step' } });

      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'myfile' } });

      const saveBtn = screen.getByRole('button', { name: 'Save' });
      fireEvent.click(saveBtn);

      await waitFor(() => {
        expect(mockOnSave).toHaveBeenCalledWith('/home/myfile.step', '.step');
      });
    });

    test('should close dialog after save', async () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'myfile' } });

      const saveBtn = screen.getByRole('button', { name: 'Save' });
      fireEvent.click(saveBtn);

      await waitFor(() => {
        expect(mockOnClose).toHaveBeenCalled();
      });
    });
  });

  describe('Integration Tests', () => {
    test('should complete full save workflow', async () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );

      // Set file name
      const input = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(input, { target: { value: 'myproject' } });

      // Select format
      const formatSelect = screen.getByLabelText('File format:') as HTMLSelectElement;
      fireEvent.change(formatSelect, { target: { value: '.step' } });

      // Click save
      const saveBtn = screen.getByRole('button', { name: 'Save' });
      fireEvent.click(saveBtn);

      await waitFor(() => {
        expect(mockOnSave).toHaveBeenCalledWith('/home/myproject.step', '.step');
        expect(mockOnClose).toHaveBeenCalled();
      });
    });

    test('should preserve path throughout interaction', () => {
      render(
        <SaveDialog isOpen={true} onClose={mockOnClose} onSave={mockOnSave} />
      );

      const pathInput = screen.getByPlaceholderText('Enter save path...') as HTMLInputElement;
      fireEvent.change(pathInput, { target: { value: '/projects/myproject' } });

      const fileInput = screen.getByPlaceholderText('Enter file name...') as HTMLInputElement;
      fireEvent.change(fileInput, { target: { value: 'design' } });

      expect(screen.getByText('/projects/myproject/design.bnd')).toBeInTheDocument();
    });
  });
});
